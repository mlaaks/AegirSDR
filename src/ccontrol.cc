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

	//std::cout << "entering control thread with do exit " << std::to_string(ctx->do_exit) << std::endl;
	usleep(16*32800); //UGLY! //this should be resolved: if this wait is removed, segfault happens...

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


