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

#include "ccontrol.h"
#include "common.h"
#include "cdsp.h"
#include "cpacketizer.h"


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
	//uint32_t					readcnt;

protected:
	//ccontrol 					*controller;
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
/*
The virtual csdrdevice base class. Could theoretically add support for
other noise source calibrated coherent SDRs in the future.
*/
//class csdrdevice{
	void set_transport(cpacketize *transport_){
		packetize=transport_;
	};
	uint32_t					readcnt;

//protected:
	ccontrol 					*controller;
	//std::thread 				thread;
	//std::mutex 				mtx;

	cpacketize					*packetize;
	
public:
	virtual int open(uint32_t index) = 0;
	virtual int open(std::string name) = 0; 
	virtual int close() = 0;
	virtual int set_fcenter(uint32_t f) = 0;
	virtual int set_samplerate(uint32_t fs) = 0;
	virtual int set_agc_mode(bool flag)=0;
	virtual int set_tuner_gain(int gain)=0;
	virtual uint32_t get_tuner_gain()=0;

	virtual int set_if_gain(int gain)=0;
	virtual uint32_t get_if_gain()=0;

	virtual int set_tuner_gain_mode(int mode)=0;
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

	const std::complex<float>* get_samplepointer(){return sfloat;};

	void request_exit(){do_exit=true;};

	inline bool exit_requested(){return do_exit;};
	
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

	csdrdevice(uint32_t blocksize_,uint32_t samplerate_, uint32_t fcenter_);
	virtual ~csdrdevice();
};
#endif
