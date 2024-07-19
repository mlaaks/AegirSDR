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

#include "csdrdevice.h"

csdrdevice::csdrdevice(uint32_t blocksize_, uint32_t samplerate_, uint32_t fcenter_) : thread(){
	blocksize = blocksize_;
	samplerate= samplerate_;
	fcenter   = fcenter_;

	realfs 	= 0;
	readcnt = 0;
	do_exit = false;
	lagrequested = true;
	lagready =false;
	synced  = false;
	newdata = 0;
	streaming=false;
	devname = "";

	lagp.lag = 0;
	lagp.ts  = 0;

	phasecorr     = std::complex<float>(1.0f,0.0f);
	phasecorrprev = std::complex<float>(1.0f,0.0f);

	int alignment = volk_get_alignment();
	sfloat 	= (std::complex<float> *) volk_malloc(sizeof(lv_32fc_t)*blocksize,alignment);

	std::memset(sfloat,0,sizeof(lv_32fc_t)*blocksize);

	controller = new ccontrol(this);
}

csdrdevice::~csdrdevice(){
	if (thread.joinable()) thread.join();

	volk_free(sfloat);
	delete controller; //why was this commented out?
}

std::complex<float> csdrdevice::est_phasecorrect(const std::complex<float> *ref){
	
	float alpha = 0.5f;
	std::complex<float> correlation=cdsp::conj_dotproduct(sfloat,ref, (blocksize >> 1));
	phasecorr  = std::conj(correlation) * (1.0f /std::abs(correlation));
	phasecorr = alpha*phasecorr + (1-alpha)*phasecorrprev;
	phasecorrprev = phasecorr;
	return phasecorr;
}

//Remove? We don't need Peak to Average Power Ratio ATM.
float csdrdevice::est_PAPR(const std::complex<float> *ref){
	return 0.0;
}

std::complex<float> csdrdevice::get_phasecorrect(){
	return phasecorr;
}

std::complex<float> *csdrdevice::phasecorrect(){
	
	cdsp::scalarmul(sfloat,sfloat,phasecorr,(blocksize>>1));
	return sfloat;
}
