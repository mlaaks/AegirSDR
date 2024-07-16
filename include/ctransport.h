/*
AegirSDR

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

coherent-rtlsdr is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AegirSDR.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <zmq.hpp>
#include <string>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <condition_variable>
#include <complex>
#include <utility>

/*
    TODO: Rename class from 'cpacketize' to 'ctransport'. When time allows, inherit
    from 'ctransport' and offer e.g. SoapySDR library support
*/

//move this inside class:
struct hdr0{
	uint32_t globalseqn;
	uint32_t N;
	uint32_t L;
	uint32_t unused;
};

/*
The packetizer singleton class. There is only one instance, it handles the ZMQ
sockets for IQ sampledata transport. One such ZMQ message contains blocksize/2
samples in column-major memory layout. The reference noise channel is always
column 0, and there are nchannels rows in the sample matrix.
*/
class ctransport{

protected:
	static ctransport*				ctransport_;
	static int 						objcount;
	static zmq::context_t 			*context;
	static zmq::socket_t  			*socket;
	static std::mutex	  			bmutex;
	static std::condition_variable 	cv;
	static bool 					noheader;
	static bool						rowmajor;
	static bool						f32bit;
	static size_t					packetlen;
	static uint32_t					nchannels;
	static uint32_t 				globalseqn;

	static int 						writecnt; //was atomic
	static bool						bufferfilled; //atomic

	static bool				 		do_exit; 	//atomic
	static uint32_t 				blocksize;

	static std::unique_ptr<int8_t[]> 				packetbuf0, packetbuf1; //samplebuffers, one is being sent, one written to
	static std::unique_ptr<std::complex<float>[]> 	packetbuf_f32_0, packetbuf_f32_1;
	static std::vector<std::complex<float>> 		pcorrection;
	

	static size_t packetlength(uint32_t N,uint32_t L);

	ctransport();
	~ctransport();

	int convert_to_rowmajor(uint32_t loc); //slow, needs to touch every value in matrix, complexity O(rows*colums)
	int convert_to_network_byte_order(uint32_t loc);
public:

	ctransport (ctransport &a) = delete;
	void operator=(const ctransport &) = delete;

	static ctransport *init(std::string address,bool noheader_,bool rowmajor_,bool f32bit_,uint32_t nchannels_,uint32_t blocksize_);
	static void cleanup();

	void request_exit();
	static int send();
	int write(uint32_t channeln,uint32_t readcnt,int8_t *rp);
	int write(uint32_t channeln,uint32_t readcnt,const std::complex<float> *in);

	int writedebug(uint32_t channeln,std::complex<float> p);

	int notifysend();
};
