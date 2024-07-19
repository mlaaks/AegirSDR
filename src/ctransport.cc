/*
AegirSDR

MIT License

Copyright (c) 2024 Mikko Laakso

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <algorithm>
#include <memory>
#include <iostream>
#include <arpa/inet.h>
#include "ctransport.h"
#include "cdsp.h"

ctransport*								ctransport::ctransport_ = nullptr;
int 									ctransport::objcount=0;
uint32_t 								ctransport::globalseqn=0;
uint32_t								ctransport::nchannels=0;
std::condition_variable 				ctransport::cv;
zmq::socket_t		 					*ctransport::socket;
zmq::context_t 							*ctransport::context;
std::mutex 								ctransport::bmutex;
std::unique_ptr<int8_t[]>				ctransport::packetbuf0;
std::unique_ptr<int8_t[]>				ctransport::packetbuf1;
std::unique_ptr<std::complex<float>[]>	ctransport::packetbuf_f32_0;
std::unique_ptr<std::complex<float>[]>	ctransport::packetbuf_f32_1;
bool 									ctransport::noheader;
bool									ctransport::rowmajor;
bool									ctransport::f32bit;
size_t 									ctransport::packetlen=0;

int 									ctransport::writecnt=0;
bool									ctransport::bufferfilled=false;
uint32_t								ctransport::blocksize=0;
bool									ctransport::do_exit=false;

std::vector<std::complex<float>> 		ctransport::pcorrection;
zmq::socket_t 							*debugsocket;

ctransport::ctransport(){
    objcount++; 			
}

ctransport::~ctransport(){
	objcount--;
	delete ctransport_;
}


    ctransport *ctransport::init(std::string address,bool noheader_,bool rowmajor_,bool f32bit_,uint32_t nchannels_,uint32_t blocksize_){
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

	packetlen  = ctransport::packetlength(nchannels,blocksize);

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

	if (ctransport_== nullptr){
		ctransport_ = new ctransport();
	}
    return ctransport_;
}

void ctransport::cleanup(){
	socket->close();
	debugsocket->close();
	delete context;
	delete socket;
	delete debugsocket;
}

void ctransport::request_exit(){
		do_exit = true;
}

size_t ctransport::packetlength(uint32_t N,uint32_t L){
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

int ctransport::send(){
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

int ctransport::writedebug(uint32_t channeln,std::complex<float> p){
	pcorrection[channeln] = p;
    return 0;
}

int ctransport::convert_to_rowmajor(uint32_t loc){
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

int ctransport::convert_to_network_byte_order(uint32_t loc){
	if (f32bit){
		std::complex<float> *p = packetbuf_f32_0.get()+loc;
		for(uint64_t i=0;i<(blocksize>>1);i++){
			p[i] = htonl(*((uint32_t *) p));
		}
	}
	return 0;
}

int ctransport::write(uint32_t channeln,uint32_t readcnt,const std::complex<float> *in){
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
    	cdsp::convto8bit((std::complex<int8_t> *) (packetbuf0.get()+loc),in,blocksize); //verify blocksize issue, define nsamples or sth.
    }
    return 0;
}

int ctransport::notifysend(){
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


