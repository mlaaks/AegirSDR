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

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unistd.h>
#include <condition_variable>
#include <complex>
#include <utility>

/*
The transport singleton class.There is only one instance.
*/
class ctransportsoapy{

	struct hdr0{
	uint32_t globalseqn;
	uint32_t N;
	uint32_t L;
	uint32_t unused;
};


protected:
	static ctransportsoapy*			ctransportsoapy_;
	static int 						objcount;
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

	ctransportsoapy();
	~ctransportsoapy();

	int convert_to_rowmajor(uint32_t loc); //slow, needs to touch every value in matrix, complexity O(rows*colums)
	int convert_to_network_byte_order(uint32_t loc);


public:

	ctransportsoapy (ctransportsoapy &a) = delete;
	void operator=(const ctransportsoapy &) = delete;

	static ctransportsoapy *init(uint32_t nchannels_,uint32_t blocksize);
	static void cleanup();

	uint32_t get_num_channels(){return nchannels;}
	uint32_t get_blocksize(){return blocksize;}


	void request_exit();
	static int send();
	int write(uint32_t channeln,uint32_t readcnt,int8_t *rp);
	int write(uint32_t channeln,uint32_t readcnt,const std::complex<float> *in);

	//int writedebug(uint32_t channeln,std::complex<float> p);

	int notifysend();
};
