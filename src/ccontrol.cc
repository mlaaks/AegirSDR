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

#include "ccontrol.h"
#include "common.h"
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

const uint32_t rtl_xtal = 28800000;

const double maxppm = TWO_POW(13)/TWO_POW(24);
const float  scale = 100.0f;
const float  frac_t = 0.90f;

//real sampling frequency, directly from rtlsdr source, credits to Osmocom Steve M.
double realfs(uint32_t requestedfs){
	uint32_t fsratio = (uint32_t)((rtl_xtal * TWO_POW(22)) / requestedfs);
	fsratio &= 0x0ffffffc;

	uint32_t real_fsratio = fsratio | ((fsratio & 0x08000000) << 1);
	return (rtl_xtal * TWO_POW(22)) / real_fsratio;
}

void ccontrol::start(){
	do_exit= false;
	thread = std::thread(&ccontrol::threadf, this);
}

void ccontrol::request_exit(){
	do_exit=true;
	dev->set_synchronized(false); // wake wait_synchronized() so the thread can see do_exit
}

void ccontrol::join(){
	thread.join();
}

//fill timestructure for nanosleep
void fillts(struct timespec* t,double ts){
	long int tnsec = ts*1e9;
	if (ts>1.0){
		t->tv_sec = floor(ts);
		t->tv_nsec= (ts - floor(ts))*1e9;
	}
	else
	{
		t->tv_sec=0;
		t->tv_nsec = tnsec; 
	}
}

//'adhoc' function to slow down samplerate change when approaching timesync
float descent(float lag){
	//return fabs(maxppm*tanh(lag/scale));
	return maxppm*tanh(lag/scale); //EDIT
}

void ccontrol::threadf(ccontrol *ctx){
	
	struct timespec tsp = {1,0L};
	long int        tns = 0L;
	time_t	 		 ts = 0;
	
	while(!ctx->do_exit){
		if(!ctx->dev->wait_synchronized()){

			ctx->dev->requestfftblocking(); //ask for a new fft.
			
			float    	lag = ctx->dev->get_lagp()->lag;
			uint32_t   	L = ctx->dev->get_blocksize();
			float		fs = realfs(ctx->dev->get_samplerate());
			float 		fcorr;

			if (fabs(lag)>sync_threshold){
				
				float      p = descent(lag);
				double	   t = frac_t*fabs(lag/(p*fs)); //time to spend at altered samplerate
				
				fillts(&tsp,t);
				
				//set the resampler frequency:	
				ctx->dev->set_correction_f(p);
				nanosleep(&tsp,NULL); //the second argument is a timepec *rem(aining)
				ctx->dev->set_correction_f(0.0f);
			}
			else{
				ctx->dev->set_correction_f(0.0f);
				ctx->dev->set_synchronized(true);
			}
		}
	}
}


