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

#ifndef CSDRDEVICEH
#define CSDRDEVICEH

#include <rtl-sdr.h>
#include <chrono>
#include <iostream>
#include <cstring>
#include <atomic>
#include <thread>

#include <volk/volk.h>
#include <fftw3.h>
#include <complex>

#include "ccontrol.h"
#include "common.h"
#include "cdsp.h"
#include "cpacketizer.h"

/*
    TODO: Edit the code to crtlsdr.cc & crefdev.cc files from header files
*/


//forward declaration, these classes have a circular dependency
class csdrdevice;
class ccontrol;

struct lagpoint{
	uint64_t ts;
	float 	 lag;
	float 	 mag;
    float    PAPR;
	lagpoint()
	{
		ts=0;
		lag=0;
		mag=0;
        PAPR=0;
	}
};


/*
The virtual csdrdevice base class. Could theoretically add support for
other noise source calibrated coherent SDRs in the future.
*/

class csdrdevice{
	uint32_t					readcnt;

protected:
	ccontrol 					*controller;
	std::thread 				thread;
	std::mutex 					mtx;
	std::condition_variable 	cv;

	std::mutex					fftmtx;
	std::condition_variable		fftcv;

	std::mutex					syncmtx;
	std::condition_variable 	synccv;

	std::atomic<int> 			newdata;
	std::atomic<bool> 			do_exit;

	std::atomic<bool>			lagrequested;
	std::atomic<bool>			lagready;
	std::atomic<bool> 			synced;
	std::atomic<bool>			streaming;

	std::complex<float>			*sfloat;

	std::complex<float>			phasecorr;
	std::complex<float>	 		phasecorrprev;

	
	uint32_t					blocksize;
	uint32_t	 				samplerate;
	uint32_t 					realfs;
	uint32_t					fcenter;
	lagpoint 					lagp;
	
	std::string					devname;
	
public:
	cpacketize					*packetize;
	
	virtual int open(uint32_t index) = 0;
	virtual int open(std::string name) = 0; 
	virtual int close() = 0;
	virtual int set_fcenter(uint32_t f) = 0;
	virtual int set_samplerate(uint32_t fs) = 0;

	virtual int set_agcmode(bool flag)=0;
	virtual int set_tunergain(uint32_t gain)=0;
	virtual int set_tunergainmode(uint32_t mode)=0;
	virtual int set_correction_f(float f)=0;

	virtual int8_t *swapbuffer(uint8_t *b) = 0;
	virtual int8_t *read()=0;

	virtual void start(barrier *b) = 0;
	virtual void startcontrol() = 0;
	virtual void stop(){};
	virtual const std::complex<float> *convtofloat() = 0;
	virtual const std::complex<float> *convtofloat(const std::complex<float>*)=0;
	virtual void consume() = 0;
    virtual uint32_t get_readcntbuf()=0;
    virtual bool is_ready() = 0;

	std::complex<float> est_phasecorrect(const std::complex<float> *ref);
	std::complex<float> get_phasecorrect();
    
    float est_PAPR(const std::complex<float> *ref);

	std::complex<float> *phasecorrect();

	void requestfft(){lagready=false; lagrequested=true;};
	void requestfftblocking(){
			lagrequested=true; 
			lagready=false; 
			{
			 std::unique_lock<std::mutex> lock(fftmtx);
			 fftcv.wait(lock,[this]{return lagready.load();});
			}
		};

	void set_lag(float lag,float mag){
		lagp.lag = lag;
		lagp.mag = mag;
		lagp.ts  = (std::chrono::time_point_cast<std::chrono::nanoseconds>
			       (std::chrono::high_resolution_clock::now())).time_since_epoch().count();

		{
        	std::lock_guard<std::mutex> lock(fftmtx);
        	lagready = true;
        	lagrequested = false;
        }

        fftcv.notify_all();
	}

	bool is_lagrequested(){
		return lagrequested;
	}

	const std::complex<float>* get_sptr(){return sfloat;};

	void request_exit(){do_exit=true;};
	
	const lagpoint* const get_lagp(){return &lagp;};
	float 	 get_samplerate(){return samplerate;};
	uint32_t get_blocksize(){return blocksize;};
	uint32_t get_fcenter(){return fcenter;};

	std::string get_devname(){return devname;};

	inline uint32_t inc_readcnt(){return readcnt++;};
	inline uint32_t get_readcnt(){return readcnt;};
	
	inline bool wait_synchronized(){
		{
			std::unique_lock<std::mutex> lock(syncmtx);
			synccv.wait(lock,[this]{return !synced.load();});
		}
		return synced;
	};

	inline void set_synchronized(bool state){
		std::lock_guard<std::mutex> lock(syncmtx);
		synced = state;
		synccv.notify_one();
	};

	inline bool get_synchronized(){
		return synced;
	};

	void set_transport(cpacketize *transport_){
		packetize=transport_;
	};

	csdrdevice(uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_);
	virtual ~csdrdevice();
};

/*
The crtlsdr class inheriting the virtual base class. Accesses librtlsdr C functions.
*/

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
	barrier			*startbarrier;
	
public:
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

	int set_agcmode(bool flag);
	int set_tunergain(uint32_t gain);
	int set_tunergainmode(uint32_t mode);
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
	const std::complex<float>* get_sptr();

	void start(barrier *b);
	void set_reference_noise_state(bool state);
	void set_bias_tee_state(uint32_t channel,bool state);

	crefsdr(uint32_t asyncbufn_,uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_) : crtlsdr(asyncbufn_,blocksize_,samplerate_,fcenter_){};
};

#endif
