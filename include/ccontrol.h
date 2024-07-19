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

#ifndef CCONTROLH
#define CCONTROLH

#include <atomic>
#include <thread>
#include "csdrdevice.h"

/* The ccontrol class: Handles timesynchronization using RTL-SDR hardware resampler.
   Synchronizes to fractional sample delays.
*/

//forward declaration, these classes have a circular dependency
class ccontrol;
class csdrdevice;

class ccontrol{
private:
	std::thread thread;
	static void threadf(ccontrol *);
	
	csdrdevice *dev;
public:
	std::atomic<bool> do_exit;

	void start();
	void request_exit();
	void join();
	ccontrol(csdrdevice *device){
		dev=device;
		do_exit = false;	
	}

	~ccontrol(){
		if (thread.joinable()) thread.join();
	}

};

#endif

