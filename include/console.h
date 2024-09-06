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
#include <sstream>
#include <csignal>
#include <atomic>
#include <map>
#include <unordered_map>
#include <thread>
#include <signal.h>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <zmq.hpp>
#include "csdrdevice.h"
#include "chackrf.h"
#include "common.h"

#include "crefnoise.h"

/*
The cconsole class. Quick and dirty local (& remote via ZMQ control socket) way
to control the receiver. To be removed and replaced in the future...
*/

using namespace std;

enum command_code{
	help,
	samplerate,
	status,
	lists,
	nop,
	logs,
	quit,
	tuningfrequency,
	request,
	phase,
	tunergain
};

//static std::map<std::string, command_code> command_codes;

const static std::unordered_map<std::string,int> command_codes{
	{"help",help},
	{"samplerate",samplerate},
	{"status",status},
	{"lists",lists},
	{"nop",nop},
	{"log",logs},
	{"quit",quit},
	{"tuningfrequency",tuningfrequency},
	{"request",request},
	{"phase",phase},
	{"tunergain",tunergain}
};

struct phistory_t{
	std::chrono::high_resolution_clock::time_point t;
	std::complex<float> p;
};


class cconsole{
private:
	barrier *startbarrier;
	int *stderr_pipe;
	csdrdevice *refdev;
	std::thread thread;
	static void consolethreadf(cconsole *);

	std::vector<phistory_t> phistory;

	crefnoise *refnoise;
public:
	lvector<csdrdevice*> *devices;

	std::atomic<bool> do_exit;
	cconsole(int p[2],crefsdr *refdev_, lvector<csdrdevice*> *devvec_,crefnoise * refnoise_);
	cconsole(int p[2],chackrfref *refdev_, lvector<csdrdevice*> *devvec_,crefnoise * refnoise_);
	~cconsole();
	void start();

	void request_exit();
	void join();

	int parsecmd(std::string);
	string getoptionstr(std::string inputln);

	int cmdlist(std::string);
	int cmdretune(std::string);
	int cmdadd(std::string);
	int cmddel(std::string);
	int cmdrequest(std::string);
	int cmdfs(std::string);
	int cmdphase(std::string);
	int cmdstatus(std::string);
	int cmdtunergain(std::string);
};