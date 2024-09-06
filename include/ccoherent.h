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

#include <vector>
#include <atomic>
#include <thread>
#include <fftw3.h>

#include "csdrdevice.h"
#include "ctransport.h"
#include "common.h"
#include "cdsp.h"
#include "crefnoise.h"


/*
 The ccoherent class. Computes cross-correlation for time synchronization. Uses
 a queue system for limiting the number of simultaneous frequency domain cross-correlations
 to support limited hardware platforms, e.g Raspberry Pi & derivatives. Also estimates
 phasecorrection factors for each channel when reference noise is enabled.
*/

//using tmpcomplextype_ = std::complex<float>; //this can be done, but is it worth it?
//using fftff_complex = tmpcomplextype_;

class ccoherent{
private:
	std::thread thread;
	static void threadf(ccoherent *);

	lvector<csdrdevice *> *devices;
	csdrdevice *refdev;

	ctransport *transport;

	fft_scheme					fftscheme,ifftscheme;
	std::complex<float>			*sifft, *sfft;   //was fftwf_complex
	std::complex<float>			*sfloat, *sconv;
	float						*smagsqr;

	int 						nfft;
	int 						blocksize;

#ifdef RASPBERRYPI
	int							mb; // RASPBERRYPI MAILBOX for gpu.
#endif

	std::vector<csdrdevice*>	lagqueue;
	crefnoise 					*refnoise;
public:

	std::atomic<bool> do_exit;
	ccoherent(csdrdevice*,lvector<csdrdevice*> *,crefnoise*, int nfft);
	~ccoherent();
	void start();
	void request_exit();
	void join();

	size_t lagqueuesize();

	void clearlagqueue();
	void queuelag(csdrdevice *d);
	void queuelag(csdrdevice *d,bool refc);
	void computelag();
};
