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

/*
The chackrf class inheriting the virtual base class. Accesses librtlsdr C functions.
*/

#pragma once
#include <libhackrf/hackrf.h>
#include <chrono>
#include <iostream>
#include <cstring>
#include <atomic>
#include <thread>

#include <volk/volk.h>
#include <fftw3.h>
#include "csdrdevice.h"


class chackrf: public csdrdevice{
	uint32_t	 	devnum;
	uint32_t		rfgain;
	bool			enableagc;
	
	static int  	asynch_callback(hackrf_transfer* transfer);

	int8_t	 		*swapbuffer(uint8_t *b);
	uint32_t		asyncbufn;

protected:
	hackrf_device	*dev;
    cbuffer         s8bit;
	static void 	asynch_threadf(chackrf *d);
	
public:
	barrier			*startbarrier;
	static uint32_t get_device_count();

	//static std::string get_device_name(uint32_t index);
	//static int         get_index_by_serial(std::string serial);
	//static std::string get_device_serial(uint32_t index);
	//static std::string get_usb_str_concat(uint32_t index);

	int open(uint32_t index);
	int open(std::string name);

	int close();
	int set_fcenter(uint32_t f);
	int set_samplerate(uint32_t fs);

	int set_agc_mode(bool flag);
	int set_tuner_gain(int gain);
	uint32_t get_tuner_gain();
	int set_if_gain(int gain);
	uint32_t get_if_gain();

	int set_tuner_gain_ext(int lna_gain, int mixer_gain,int vga_gain);

	int set_tuner_gain_mode(int mode);

	int set_correction_f(float f);
	void set_bias_tee_state(uint32_t,bool);

	uint32_t get_asyncbufn(){return asyncbufn;};

	bool is_ready(){
		return ((get_readcnt() >= get_asyncbufn()) && streaming);
	};

	const std::complex<float> *convtofloat(); //needed for overloading in case of reference channel.
	const std::complex<float> *convtofloat(const std::complex<float>*);

	void start(barrier *b);
	void startcontrol();
	void stop();
    
    uint32_t get_readcntbuf(){
        return s8bit.get_rcnt();
    };
    
	int8_t *read();
	void consume(){s8bit.consume();};

	chackrf(uint32_t asyncbufn_,uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_) : csdrdevice(blocksize_,samplerate_,fcenter_), s8bit(asyncbufn_,blocksize_){
		asyncbufn = asyncbufn_;
		rfgain    = 0;
	};
	~chackrf(){
		hackrf_exit();
	};
};


/*
The crefsdr class. In this implementation, the reference channel is a special case, which in 
the original coherent-rtlsdr was not use for signals. I.e., it only captured the common refecence noise
*/

class chackrfref: public chackrf{
	/*needed for overloading in case of reference channel. this just places the data in the second half of the buffer.
	  This is a trick for frequency domain (circular) cross-correlation, which saves some CPU cycles.
	*/
public:
	const std::complex<float> *convtofloat();
	const std::complex<float> *convtofloat(const std::complex<float>*);

	void start(barrier *b);
	void set_reference_noise_state(bool state);
	void set_bias_tee_state(uint32_t channel,bool state);

	chackrfref(uint32_t asyncbufn_,uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_) : chackrf(asyncbufn_,blocksize_,samplerate_,fcenter_){};
};