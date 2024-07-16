/*
AegirSDR

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

coherent-rtlsdr is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See thFbe
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AegirSDR.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
The crtlsdr class inheriting the virtual base class. Accesses librtlsdr C functions.
*/

#pragma once
#include <rtl-sdr.h>
#include <chrono>
#include <iostream>
#include <cstring>
#include <atomic>
#include <thread>

#include <volk/volk.h>
#include <fftw3.h>
#include "csdrdevice.h"


class crtlsdr: public csdrdevice{
	uint32_t	 	devnum;
	uint32_t		rfgain;
	bool			enableagc;
	
	static void 	asynch_callback(unsigned char *buf, uint32_t len, void *ctx);

	int8_t	 		*swapbuffer(uint8_t *b);
	uint32_t		asyncbufn;

protected:
	rtlsdr_dev_t	*dev;
    cbuffer         s8bit;
	static void 	asynch_threadf(crtlsdr *d);
	
public:
	barrier			*startbarrier;
	static uint32_t get_device_count();

	static std::string get_device_name(uint32_t index);
	static int         get_index_by_serial(std::string serial);
	static std::string get_device_serial(uint32_t index);
	static std::string get_usb_str_concat(uint32_t index);

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

	crtlsdr(uint32_t asyncbufn_,uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_) : csdrdevice(blocksize_,samplerate_,fcenter_), s8bit(asyncbufn_,blocksize_){
		asyncbufn = asyncbufn_;
		rfgain    = 0;
	};
	~crtlsdr(){
	};
};


/*
The crefsdr class. In this implementation, the reference channel is a special case, which in 
the original coherent-rtlsdr was not use for signals. I.e., it only captured the common refecence noise
*/

class crefsdr: public crtlsdr{
	/*needed for overloading in case of reference channel. this just places the data in the second half of the buffer.
	  This is a trick for frequency domain (circular) cross-correlation, which saves some CPU cycles.
	*/
public:
	const std::complex<float> *convtofloat();
	const std::complex<float> *convtofloat(const std::complex<float>*);

	void start(barrier *b);
	void set_reference_noise_state(bool state);
	void set_bias_tee_state(uint32_t channel,bool state);

	crefsdr(uint32_t asyncbufn_,uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_) : crtlsdr(asyncbufn_,blocksize_,samplerate_,fcenter_){};
};