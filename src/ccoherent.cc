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
#include <iostream>
#include <mutex>
#include "ccoherent.h"
#include "unistd.h"

#ifdef RASPBERRYPI
int uint32log2(uint32_t k){
	int i=0;
	for (i=-1;k>=1;k>>=1,++i);
	return i;
}
#endif

ccoherent::ccoherent(crefsdr* refdev_,lvector<csdrdevice*> *devvec_,crefnoise* refnoise_, int nfft_){
	refdev =refdev_;
	devices=devvec_;
	refnoise=refnoise_;
	do_exit=false;
	nfft = nfft_;

	int alignment = volk_get_alignment();

	blocksize = refdev->get_blocksize();
	uint32_t bufferlen = blocksize*nfft;
	
	smagsqr	= (float *) volk_malloc(sizeof(float)*bufferlen,alignment);
	std::memset(smagsqr,0,sizeof(float)*bufferlen);

	// create plans for fft transforms, perhaps move these to cdsp someday.
#ifdef RASPBERRYPI
	int ret;
	int log2_N = uint32log2(blocksize);
	mb  = mbox_open();
	ret = gpu_fft_prepare(mb,log2_N,GPU_FFT_FWD,nfft,&fftscheme);
	if (ret){
		cout << "Failed to create "<< to_string(nfft) <<  "X gpu_fft_forward for log2N: " << to_string(log2_N) << endl;
	}
	ret = gpu_fft_prepare(mb,log2_N,GPU_FFT_REV,nfft,&ifftscheme); //perhaps check return values some day...
	if (ret){
		cout << "Failed to create "<< to_string(nfft) <<  "X gpu_fft_inverse for log2N: " << to_string(log2_N) << endl;
	}
	
#else

	//alloc buffers for dsp. note, fftwf_complex is bin. compat with lv_32fc_t and std::complex<float>.
	sfft  = (std::complex<float> *) fftwf_alloc_complex(bufferlen);
	sifft = (std::complex<float> *) fftwf_alloc_complex(bufferlen);
	sfloat 	= (std::complex<float> *) volk_malloc(sizeof(lv_32fc_t)*bufferlen,alignment);
	sconv	= (std::complex<float> *) volk_malloc(sizeof(lv_32fc_t)*bufferlen,alignment);

	//zero all buffers. important especially for circular convolution.
	std::memset(sfft,0,sizeof(fftwf_complex)*bufferlen);
	std::memset(sifft,0,sizeof(fftwf_complex)*bufferlen);
	std::memset(sfloat,0,sizeof(lv_32fc_t)*bufferlen);
	std::memset(sconv,0,sizeof(lv_32fc_t)*bufferlen);

	// fftw requires a plan:
	int rank =1;
	int n[] = {blocksize};
	int *inembed = n;
	int *onembed= n;
	int idist = blocksize;
	int odist = blocksize;
	int istride=1;
	int ostride=1;

	fftscheme  = fftwf_plan_many_dft(rank, n, nfft, (fftwf_complex *) sfloat,
								  inembed,istride,idist,(fftwf_complex *) sfft,
								  onembed,ostride,odist,FFTW_FORWARD,0);
	
	ifftscheme = fftwf_plan_many_dft(rank, n, nfft, (fftwf_complex *) sconv,
								  inembed,istride,idist,(fftwf_complex *) sifft,
								  onembed,ostride,odist,FFTW_BACKWARD,0);

#endif
}

ccoherent::~ccoherent(){
	if (thread.joinable()) thread.join();

	volk_free(smagsqr);
#ifdef RASPBERRYPI
	gpu_fft_release(fftscheme);
	gpu_fft_release(ifftscheme);
#else
	volk_free(sfloat);
	volk_free(sconv);
	fftwf_free(sfft);
	fftwf_free(sifft);
	fftwf_destroy_plan(fftscheme);
	fftwf_destroy_plan(ifftscheme);
#endif

}

void ccoherent::clearlagqueue(){
	lagqueue.clear();
}

size_t ccoherent::lagqueuesize(){
	return lagqueue.size();
}

void ccoherent::queuelag(csdrdevice *d){
	if (lagqueue.size()<nfft){
#ifdef RASPBERRYPI
		std::complex<float> *inptr = (std::complex<float> *) (fftscheme->in + lagqueue.size()*fftscheme->step);
		std::memcpy(inptr,d->get_samplepointer(),sizeof(std::complex<float>)*(blocksize));
#else
		std::memcpy((sfloat+lagqueue.size()*blocksize), d->get_samplepointer(),sizeof(std::complex<float>)*(blocksize)); //fix me, half of the copy is zeros.
#endif
		lagqueue.push_back(d);
	}
}
void ccoherent::queuelag(csdrdevice *d,bool refc){
	if (lagqueue.size()<nfft){
		if (refc){
			d->convtofloat(sfloat+lagqueue.size()*blocksize);
		}
		else{
			d->convtofloat(sfloat+lagqueue.size()*blocksize);
		}
		lagqueue.push_back(d);
	}
}

void ccoherent::computelag()
{

#ifdef RASPBERRYPI
	
	cdsp::fft(&fftscheme);
	std::complex<float> *refptr = (std::complex<float> *) fftscheme->out;
	for (int k=1;k<lagqueue.size();k++){
		std::complex<float> *outptr = (std::complex<float> *) (fftscheme->out + k*fftscheme->step);
		std::complex<float> *inptr = (std::complex<float> *) (ifftscheme->in + k*ifftscheme->step);
		cdsp::conjugatemul(inptr,outptr,refptr,blocksize);
	}	
	cdsp::fft(&ifftscheme);

	for (int k=1;k<lagqueue.size();k++){
		std::complex<float> *outptr = (std::complex<float> *) (ifftscheme->out + k*ifftscheme->step);
		cdsp::magsquared(smagsqr+k*blocksize,outptr,blocksize);
	}
#else
	cdsp::fft(sfft,sfloat,&fftscheme); //nfft fft:s

	//multiply each with ref
	for(int k=1;k<lagqueue.size();k++){
		cdsp::conjugatemul(sconv+k*blocksize,sfft+k*blocksize,sfft,blocksize);
	}

	//nfft inverse fft:s
	cdsp::fft(sifft,sconv,&ifftscheme);

	//squared magnitude for all except ref
	cdsp::magsquared(smagsqr+blocksize,sifft+blocksize,blocksize*(nfft-1));
#endif

	for(int k=1;k<lagqueue.size();k++){
		float    lag=0.0f;
		float    D=0.0f,a=0.0f,b=0.0f;
		uint32_t idx = cdsp::indexofmax(smagsqr + k*blocksize,blocksize);
		
		float *ptr = (smagsqr + k*blocksize + idx);			// this should be taken from sifft() real part.
		float mag  = sqrt( *(ptr) / float(blocksize>>1) );  // not quite sure about the scaling. fftw is unnormalized, so ifft(fft(x)) = N*x
		
		if ((idx>1) && (idx<(blocksize-1))){ //check that n-1 and n+1 won't go off bounds
			a=-*(ptr-1)-2*(*ptr)+*(ptr+1);
			b=-0.5f*(*(ptr-1))+0.5f*(*(ptr+1));

			//handle possible divide by zero by setting the fractional to zero:
			if (a!=0.0f){
				D=-b/a;
			}
			else{
				D=0.0f;
				cout << "frac zeroed at 1" <<endl;
			}
			
			lag = idx + D;
			}
		else{
			lag = idx;
		}

		lag-=(blocksize >> 1);
		lagqueue[k]->set_lag(lag,mag);
	}
	lagqueue.clear();
}

void ccoherent::join(){
	thread.join();
}

void ccoherent::threadf(ccoherent *ctx){

	while(!ctx->do_exit){

		//read reference channel and perform fft:
		ctx->clearlagqueue();
		int8_t *refsptr = ctx->refdev->read();
		std::complex<float> *ref_f32 = (std::complex<float> *) ctx->refdev->convtofloat();
		ctx->queuelag(ctx->refdev);
		ctx->refdev->transport->write(0,ctx->refdev->get_readcntbuf(),ref_f32);
		ctx->refdev->consume();

		bool allsynced=true;
		for(auto *d:*ctx->devices){
			if(!d->get_synchronized()){
				allsynced=false;
				break;
			};
		}
		if (allsynced){
			int c=1;
			for(auto *d:*ctx->devices){
				d->read();
				if(d->is_ready()){
				d->convtofloat();
				d->phasecorrect();
				d->transport->write(c,d->get_readcntbuf(),d->get_samplepointer());
				d->transport->writedebug(c,d->get_phasecorrect());
				c++;
				d->consume();
				}
			}
		}
		else{
			int c=1;
			for(auto *d:*ctx->devices){
				d->read();
				if(d->is_ready()){
					std::complex<float> *sfloat = (std::complex<float> *) d->convtofloat();
					if (d->is_lagrequested()){
						ctx->queuelag(d);
					}
					
					if (ctx->refnoise->isenabled()){
						std::complex<float> p = d->est_phasecorrect(ctx->refdev->get_samplepointer()+(ctx->refdev->get_blocksize()>>1));
					}

					d->phasecorrect();
					d->transport->write(c,d->get_readcntbuf(),d->get_samplepointer());
					d->transport->writedebug(c,d->get_phasecorrect());
					c++;
					d->consume();
				}
			}
		}
		if (ctx->lagqueuesize()>1){
			ctx->computelag();
		}
		ctx->refdev->transport->notifysend();
	}
}

void ccoherent::start(){
	thread = std::thread(&ccoherent::threadf,this);
}

void ccoherent::request_exit(){
	do_exit=true;
}
