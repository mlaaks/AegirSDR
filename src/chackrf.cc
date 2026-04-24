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

#include "chackrf.h"		
#include "common.h"
#include <unistd.h>
#include <csignal>


void chackrf::start(barrier *b){
	startbarrier = b;
	streaming = true;
	thread = std::thread(chackrf::asynch_threadf,this);
	startcontrol();
}

void chackrf::startcontrol(){
	controller->start();
}

void chackrf::stop(){
	streaming = false;
	cv.notify_all();
	controller->request_exit();
	std::raise(SIGUSR1);
}

void chackrf::asynch_threadf(chackrf *d){
	int ret;
	//std::cout << "starting #" << std::to_string(d->devnum) <<", asyncbufn:" << std::to_string(d->get_asyncbufn()) << std::endl;
	
	d->startbarrier->wait();
	hackrf_start_rx(d->dev, chackrf::asynch_callback, (void*) d);
	std::cout << "starting #" << std::to_string(d->devnum) <<", asyncbufn:" << std::to_string(d->get_asyncbufn()) << std::endl;
}

/*
 * From rtl-sdr.h in https://github.com/krakenrf/librtlsdr
 * Due to incomplete concurrency implementation, cancel_async should
 * only be called from within the callback function, so it is
 * in the correct thread.*/
/*
int rx_callback(hackrf_transfer* transfer){
    //fprintf(stderr,"Read %d bytes\n",transfer->valid_length);
    pthread_mutex_lock(&mutex);
    bytes_read+=transfer->valid_length;
    memcpy(b0,transfer->buffer,transfer->valid_length);
    w0=transfer->valid_length;

    uint32_t tw=w0;
    w0 = w1;
    w1 = tw;

    int8_t *tmp = b0;
    b0 = b1;
    b1 = tmp;

    pthread_mutex_unlock(&mutex);

    return 0;
}*/

int chackrf::asynch_callback(hackrf_transfer* transfer)
{
	chackrf *d = (chackrf *)transfer->rx_ctx;
	//std::cout << "receiving" << std::to_string(d->devnum) << " length:"<< std::to_string(transfer->valid_length)<<std::endl;
	
	if (d) {
		
		if (d->exit_requested()){
			hackrf_stop_rx(d->dev);
			std::cout<<"stoping streaming"<<std::endl;
		}

		d -> swapbuffer(transfer->buffer);
	}
	return 0;
}

uint32_t chackrf::get_device_count(){
	hackrf_device_list_t *dl;
    int ret = hackrf_init();
    if (!ret){
	    dl = hackrf_device_list();
	    ret = dl->devicecount;
	    hackrf_device_list_free(dl);
	}
	return ret;
}
/*
std::string chackrf::get_device_name(uint32_t index){
	return std::string("0");
}*/
/*
int crtlsdr::get_index_by_serial(std::string serial){
	return rtlsdr_get_index_by_serial(serial.data());
}

std::string crtlsdr::get_device_serial(uint32_t index){
	char manufact[256];
	char product[256];
	char serial[256];
	int  ret = rtlsdr_get_device_usb_strings(index,manufact,product,serial);
	if (ret<0){
		std::cerr << "get_device_serial() failed for #"<< std::to_string(index) << std::endl;
		return "";
	}

	return serial;
}

std::string crtlsdr::get_usb_str_concat(uint32_t index){
	char manufact[256];
	char product[256];
	char serial[256];
	int  ret = rtlsdr_get_device_usb_strings(index,manufact,product,serial);
	if (ret<0){
		std::cerr << "get_device_serial() failed for #"<< std::to_string(index) << std::endl;
		return "";
	}

	return std::string(serial) +" : " + std::string(product) + " : " + std::string(manufact);
}
*/
int chackrf::open(uint32_t n){
	return 0;
}
int chackrf::open(std::string name){
	int ret = hackrf_open_by_serial(name.data(),&dev);
	if (ret!=0) return ret;
	ret=set_samplerate(samplerate);
	if (ret!=0) return ret;
	ret=set_fcenter(fcenter);

	return ret;
}

int chackrf::close(){
	hackrf_close(dev);
	dev = NULL;
	return 0;
}


int chackrf::set_fcenter(uint32_t f){
	fcenter=f;
	return hackrf_set_freq(dev,f);
}

int chackrf::set_samplerate(uint32_t fs){
	samplerate = fs;
	return hackrf_set_sample_rate(dev,fs);
}

int chackrf::set_agc_mode(bool flag){
	return 0;
}

int chackrf::set_tuner_gain(int gain){
	rfgain = gain;
	return gain;
	//cout << "Trying to set tuner gain " << to_string(devnum) << endl;
	//return rtlsdr_set_tuner_gain(dev, gain);
}

/*!
 * Set LNA / Mixer / VGA Device Gain for R820T/2 device is configured to.
 *
 * \param dev the device handle given by rtlsdr_open()
 * \param lna_gain index in 0 .. 15: 0 == min;   see tuner_r82xx.c table r82xx_lna_gain_steps[]
 * \param mixer_gain index in 0 .. 15: 0 == min; see tuner_r82xx.c table r82xx_mixer_gain_steps[]
 * \param vga_gain index in 0 .. 15: 0 == -12 dB; 15 == 40.5 dB; => 3.5 dB/step;
 *        vga_gain index 16 activates AGC for VGA controlled from RTL2832
 *     see tuner_r82xx.c table r82xx_vga_gain_steps[]
 * \return 0 on success
 */


int chackrf::set_tuner_gain_ext(int lna_gain, int mixer_gain, int vga_gain){
	return 0;
}

uint32_t chackrf::get_tuner_gain(){
	return 0;
}

uint32_t chackrf::get_if_gain(){
	return 0; //no such function in rtl_sdr.h 
}



/*!
 * Set the agc_variant for automatic gain mode for the device (only R820T/2).
 * Automatic gain mode must be enabled for the gain setter function to work.
 *
 * \param dev the device handle given by rtlsdr_open()
 * \param if_mode:
 *         0           set automatic VGA, which is controlled from RTL2832
 *     -2500 .. +2500: set fixed IF gain in tenths of a dB, 115 means 11.5 dB.
 *                     use -1 or +1 in case you neither want attenuation nor gain.
 *                     this equals the VGA gain for R820T/2 tuner.
 *                     exact values (R820T/2) are in range -47 .. 408 in tenth of a dB,
 *                       giving -4.7 .. +40.8 dB. these exact values may slightly change
 *                       with better measurements.
 *     10000 .. 10015: IF gain == VGA index from parameter if_mode
 *                     set if_mode by index: index := VGA_idx +10000
 *     10016 .. 10031: same as 10000 .. 10015, but additionally set automatic VGA
 *     10011:          for fixed VGA (=default) of -12 dB + 11 * 3.5 dB = 26.5 dB
 * 
 * \return 0 on success
 */

int chackrf::set_if_gain(int gain){
	return 0;
}

int chackrf::set_tuner_gain_mode(int mode){
	return 0;
}

int chackrf::set_correction_f(float f){
	return 0;
}


int8_t* chackrf::swapbuffer(uint8_t *b){

	//do we need buffer timestamps like USRP/UHD devices do?

	//auto t = std::chrono::high_resolution_clock::now();
	//auto t_ns= (std::chrono::time_point_cast<std::chrono::nanoseconds>(t)).time_since_epoch();

	//if (streaming){
		{
		    std::unique_lock<std::mutex> lock(mtx);
			s8bit.setbufferptr(b,get_readcnt()); //postincremented
			inc_readcnt(); //was included in mutex scope.		 		
	 		newdata++;
		}
	//}
	cv.notify_all();
	//std::cout << "swapbuffer" << std::endl;
	
	return s8bit.getbufferptr();
}

int8_t* chackrf::read(){
	if (streaming){
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this]{return (newdata.load()>0);});
		newdata--;
		return s8bit.getbufferptr(); //this has to be inside mutex scope, otherwise swapbuffer may modify reaccnt and we return wrong buffer occasionally.
	}
	return s8bit.getbufferptr();
}

const std::complex<float> *chackrf::convtofloat(){
	return cdsp::convtofloat(sfloat,s8bit.getbufferptr(),blocksize);
}

const std::complex<float> *chackrf::convtofloat(const std::complex<float> *p){
	return cdsp::convtofloat(p,s8bit.getbufferptr(),blocksize);
}

void chackrf::set_bias_tee_state(uint32_t channel,bool state){
	int ret;
}

const std::complex<float> *chackrfref::convtofloat(){
	cdsp::convtofloat(sfloat+(blocksize>>1),s8bit.getbufferptr(),blocksize);
	return sfloat + (blocksize>>1);
}

const std::complex<float> *chackrfref::convtofloat(const std::complex<float> *p){
	cdsp::convtofloat(p+(blocksize>>1),s8bit.getbufferptr(),blocksize);
	return p + (blocksize >> 1);
}

void chackrfref::start(barrier *b){
	startbarrier = b;
	thread = std::thread(chackrf::asynch_threadf,this);
	streaming = true;
}

void chackrfref::set_reference_noise_state(bool state){
}

void chackrfref::set_bias_tee_state(uint32_t channel,bool state){
	int ret;
}