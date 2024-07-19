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
#include <volk/volk.h>
#include <volk/constants.h>
//#include <arm_neon.h> //Raspberry Pi

/*
The cdsp class: currently maps to VOLK kernels, thanks GNU Radio. In 
the future, implementations can be changed if need be.
*/

#ifdef RASPBERRYPI
extern "C"{
	#include "gpu_fft.h"
	#include "mailbox.h"
	typedef struct GPU_FFT *fft_scheme;
}
#else
	#include <fftw3.h>
	typedef fftwf_plan fft_scheme;
#endif

class cbuffer;

class cdsp{
public:
	//static void convtosigned(const cbuffer in, const cbuffer out, int n);
	//static void convtosigned(const uint8_t*in, const cbuffer out, int n);
	static void convtosigned(const uint8_t*in, const uint8_t *out, int n);
	static const float* convtofloat(const float *out, const int8_t *s8bit, int n);

	static const std::complex<int8_t>* convto8bit(std::complex<int8_t> *out,const std::complex<float>*in, int n);

	static const std::complex<float>* convtofloat(const std::complex<float> *out, const int8_t *s8bit, int n);

	static const std::complex<float>* scalarmul(const std::complex<float> *out,const std::complex<float> *in,const std::complex<float> scalar_in, int n);
    
    static const std::complex<float> conj_dotproduct(const std::complex<float> *a,const std::complex<float> *b, int n);
	//static const std::complex<float>* convtofloat(const float *out, const int8_t *s8bit, int n);

	static const float rms(const float *in, int n);
	static const float rms(const std::complex<float> *in,int n);
    
    static const float PAPR(const std::complex<float> *s,const std::complex<float> *ref, int n);

	static const float crestfactor(const float *in,float peak, int n);
	static const float crestfactor(const float *in, int n);

	static const float* magsquared(float *out,const std::complex<float> *in,int n);

	static const std::complex<float>* conjugatemul(std::complex<float>*out, std::complex<float> *in1, std::complex<float> *in2, int n);

	static const std::complex<float>* fft(std::complex<float>*out, std::complex<float> *in,fft_scheme *scheme);
	static const std::complex<float>* fft(fft_scheme *scheme);

	static const uint32_t indexofmax(float *in,int n);
	static const uint32_t indexofmax(float *out,std::complex<float> *in,int n);
	//static const std::complex<float>* ifft(std::complex<float>*out, std::complex<float> *in);

	
	static const std::string implementation();
	static const std::string version();

};
