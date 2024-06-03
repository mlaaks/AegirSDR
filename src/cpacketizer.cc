/*
coherent-rtlsdr

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

coherent-rtlsdr is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with coherent-rtlsdr.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <memory>
#include <iostream>
#include <arpa/inet.h>
#include "cpacketizer.h"
#include "cdsp.h"

cpacketize*								cpacketize::cpacketize_ = nullptr;
int 									cpacketize::objcount=0;
uint32_t 								cpacketize::globalseqn=0;
uint32_t								cpacketize::nchannels=0;
std::condition_variable 				cpacketize::cv;
zmq::socket_t		 					*cpacketize::socket;
zmq::context_t 							*cpacketize::context;
std::mutex 								cpacketize::bmutex;
std::unique_ptr<int8_t[]>				cpacketize::packetbuf0;
std::unique_ptr<int8_t[]>				cpacketize::packetbuf1;
std::unique_ptr<std::complex<float>[]>	cpacketize::packetbuf_f32_0;
std::unique_ptr<std::complex<float>[]>	cpacketize::packetbuf_f32_1;
bool 									cpacketize::noheader;
bool									cpacketize::rowmajor;
bool									cpacketize::f32bit;
size_t 									cpacketize::packetlen=0;

int 									cpacketize::writecnt=0;
bool									cpacketize::bufferfilled=false;
uint32_t								cpacketize::blocksize=0;
bool									cpacketize::do_exit=false;

std::vector<std::complex<float>> 		cpacketize::pcorrection;
zmq::socket_t 							*debugsocket;

cpacketize::cpacketize(){
    objcount++; 			
}

cpacketize::~cpacketize(){
	objcount--;
	delete cpacketize_;
}


cpacketize *cpacketize::init(std::string address,bool noheader_,bool rowmajor_,bool f32bit_,uint32_t nchannels_,uint32_t blocksize_){
	context = new zmq::context_t(1);
	socket  = new zmq::socket_t(*context,ZMQ_PUB);
	socket->bind(address.data());
	noheader = noheader_;
	blocksize= blocksize_;
	rowmajor = rowmajor_;
	nchannels=nchannels_;
	f32bit = f32bit_;

	debugsocket = new zmq::socket_t(*context,ZMQ_PUB);
	debugsocket->bind("tcp://*:5557");

	packetlen  = cpacketize::packetlength(nchannels,blocksize);

	if (f32bit){
		packetbuf_f32_0 = std::make_unique<std::complex<float> []>(packetlen);
		packetbuf_f32_1 = std::make_unique<std::complex<float> []>(packetlen);

		std::cout << "Reserved buffers of " << std::to_string(packetlen) << std::endl;
	}
	else{
		packetbuf0 = std::make_unique<int8_t[]>(packetlen);
		packetbuf1 = std::make_unique<int8_t[]>(packetlen);
	}

	pcorrection.resize(nchannels_,std::complex<float>(0.0f,0.0f));

	if (cpacketize_==nullptr){
		cpacketize_ = new cpacketize();
	}
    return cpacketize_;
}

void cpacketize::cleanup(){
	socket->close();
	debugsocket->close();
	delete context;
	delete socket;
	delete debugsocket;
}

void cpacketize::request_exit(){
		do_exit = true;
}

size_t cpacketize::packetlength(uint32_t N,uint32_t L){
	if (f32bit){
		if (noheader){
			return (N*(L>>1)*sizeof(std::complex<float>));
		}
		else{
			return (sizeof(hdr0) + sizeof(uint32_t)*N) + (N*(L>>1)*sizeof(std::complex<float>));
		}
	}
	else{
		if (noheader){
			return (2*N*L);
		}
		else{
			return (sizeof(hdr0) + sizeof(uint32_t)*N) + 2*N*L;
		}
	}
}

int cpacketize::send(){
	if (!noheader){
		//fill static header. block readcounts filled by calls to write:
		hdr0 *hdr 		= f32bit ? (hdr0 *) packetbuf_f32_0.get() : (hdr0 *) packetbuf0.get();
		hdr->globalseqn	= globalseqn++;
		hdr->N 			= nchannels;
		hdr->L 			= blocksize >> 1;
		hdr->unused 	= 0;
	}
		
	{
	    std::unique_lock<std::mutex> lock(bmutex);
	    cv.wait(lock,[]{return ((bufferfilled) || (do_exit));});
	    bufferfilled = false;
	}

	if (f32bit){
		socket->send(packetbuf_f32_1.get(),packetlen,0); //zmq::send_flags::none
	}
	else{
    	socket->send(packetbuf1.get(),packetlen,0);
    }

    //debug
    debugsocket->send(pcorrection.data(),nchannels*sizeof(std::complex<float>),0);
    return 0;
}

int cpacketize::writedebug(uint32_t channeln,std::complex<float> p){
	pcorrection[channeln] = p;
    return 0;
}

int cpacketize::convert_to_rowmajor(uint32_t loc){
	std::complex<float> *p = packetbuf_f32_0.get();
	std::complex<int8_t> *p8bit = (std::complex<int8_t> *) packetbuf0.get();

	if(f32bit){
		for (int r=0;r<nchannels;r++){
			for (int c=0;c<(blocksize>>1);c++){
				*(p + loc + r*(blocksize>>1)+c) = p[c*nchannels+r];				
			}
		}
	}
	else{
		for (int r=0;r<nchannels;r++){
			for (int c=0;c<(blocksize>>1);c++){
				*(p8bit + loc + r*(blocksize>>1)) = p8bit[c*nchannels+r];
			}
		}
	}

	return 0;
}

int cpacketize::convert_to_network_byte_order(uint32_t loc){
	if (f32bit){
		std::complex<float> *p = packetbuf_f32_0.get()+loc;
		for(uint64_t i=0;i<(blocksize>>1);i++){
			p[i] = htonl(*((uint32_t *) p));
		}
	}
	return 0;
}

int cpacketize::write(uint32_t channeln,uint32_t readcnt,const std::complex<float> *in){
    uint32_t loc;

    if (noheader){
        loc = f32bit? channeln*(blocksize>>1): channeln*blocksize;
    }
    else{
        //fill dynamic size part of header, write readcounts
        if (f32bit){
        	*(((uint32_t *)packetbuf_f32_0.get()) + sizeof(hdr0)/sizeof(uint32_t)+channeln)=readcnt;
        	loc = (sizeof(hdr0)+nchannels*sizeof(uint32_t)) + channeln*(blocksize>>1);
        }
        else{
        	*(((uint32_t *)packetbuf0.get()) + sizeof(hdr0)/sizeof(uint32_t)+channeln)=readcnt;
        	loc = (sizeof(hdr0)+nchannels*sizeof(uint32_t)) + channeln*blocksize;
        }
    }

    if(f32bit){
    	std::memcpy((std::complex<float> *) packetbuf_f32_0.get()+loc,in,(blocksize>>1)*sizeof(std::complex<float>));
    	
    	if(rowmajor){
    		convert_to_rowmajor(loc);
    	}
    }
    else{
    	cdsp::convto8bit((std::complex<int8_t> *) (packetbuf0.get()+loc),in, (blocksize>>1));
    }
    return 0;
}

int cpacketize::notifysend(){
	std::unique_lock<std::mutex> lock(bmutex);

	if (f32bit){
		packetbuf_f32_0.swap(packetbuf_f32_1);
	}
	else{
		packetbuf0.swap(packetbuf1);
	}
    writecnt = nchannels;

	bufferfilled = true;
	lock.unlock();

	cv.notify_one();
    return 0;
} 


