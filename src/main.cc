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

#include <iostream>
#include <sstream>
#include <csignal>
#include <atomic>
#include <thread>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <mutex>

#include "csdrdevice.h"
#include "crtlsdr.h"
#include "console.h"
#include "ccoherent.h"
#include "ctransport.h"
#include "common.h"
#include "crefnoise.h"
#include "cconfigfile.h"
#include "cdsp.h"

using namespace std;
int stderr_pipe[2];		//for redirecting stderr, where librtlsdr outputs
FILE *stderr_f;

std::atomic<bool> exit_all;

void signal_handler(int signal)
{
  if (signal==SIGINT){
  	cout << endl << "caught CTRL+C (SIGINT), exiting after CTRL+D..." <<endl; //std::to_string(signal)
  	exit_all=true;
  }
}
void int_handler(int signal)
{
	if (signal==SIGUSR1){
		//dummy handler, for waking up sleeping control threads...
	}
}

//redirect C fprintf(stder,...) to a pipe, suppress stdout output from librtlsdr
void redir_stderr(bool topipe){
	int ret = pipe(stderr_pipe);	
	stderr_f = fdopen(stderr_pipe[0],"r");
	dup2(stderr_pipe[1],2);
	close(stderr_pipe[1]);
	fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);
	//freopen ("stderrlog","w",stderr); //redirect to a file...
}

struct cl_ops{
	string   refname;
	bool     agc;
	uint32_t fs;
	uint32_t fc;
	uint32_t asyncbufn;
	uint32_t blocksize;
	int		 	 ndev;
	uint32_t gain;
	uint32_t refgain;
	bool		 no_header;
	string 	 config_fname;
	bool	 	 use_cfg;
	bool	 	 quiet;
	bool		 rowmajor;		  // mostly for GNU Radio ZMQ source.
	bool		 use32bitfloat; // e.g. gr_complex, std::complex<float>, interleaved 32bit float
	bool		 krakensdr;
	bool		 krakenbiastee;
};

void usage(void)
{
	fprintf(stderr,
		"\ncoherentsdr - synchronous rtl-sdr reader, IQ-samples published to a ZMQ-socket (atm, this is tcp://*:5555)\n"
		"Dongles need to be clocked from the same signal, otherwise cross-correlation and synchronization will result in garbage\n\n"
		"Usage:\n"
		"\t[-f frequency_to_tune_to [Hz'(default 480MHz)]\n"
		"\t[-b blocksize [samples'(default 2^14=16384, use 2^n)]\n"
		"\t[-s samplerate (default: 2048000 Hz)]\n"
		"\t[-n number of devices to init\n"
		"\t[-g tuner gain: signal [dB] (default 50)]\n"
		"\t[-r tuner gain: reference [dB] (default 50)]\n"
		"\t[-I reference dongle serial ID (default '1000')]\n"
		"\t[-A set automatic gaincontrol for all devices]\n"
		"\t[-C 'config file', read receiver config from a file]\n"
		"\t[-q quiet mode, redirect stderr from rtl-sdr\n"
		"\t[-R outputmode raw: [no packet header.]\n"
		"\t[-w wireformat: 8 or 32 [int8 (default) or complex<float> (32 bit)]\n"
		"\t[-m memorylayout (for transport): row-major [default: column-major]\n"
		"\t[-K KrakenSDR device support (experimental)\n"
		"\t[-B KrakenSDR enable bias tees\n");
	exit(1);
}

int parsecommandline(cl_ops *ops, int argc, char **argv){
	int opt;
	while ((opt = getopt(argc, argv, "s:b:f:h:n:g:r:I:C:w:mARqKB")) != -1) {
		switch (opt) {
			case 's':
					ops->fs=(uint32_t)atof(optarg);
				break;
			case 'b':
					ops->blocksize = (uint32_t)atoi(optarg);
				break;
			case 'f':
					ops->fc=(uint32_t) atof(optarg);
				break;
			case 'h':
					usage();
				break;
			case 'n':
					if ((uint32_t)atoi(optarg)<=ops->ndev)
						ops->ndev =(uint32_t)atoi(optarg);
					else
						cout << "Requested device count higher than devices connected to system " 
						<< to_string(ops->ndev) << ", setting ndev=" << to_string(ops->ndev) << endl;  
				break;
			case 'g':
					ops->gain = (uint32_t) atoi(optarg)*10;
				break;
			case 'r':
					ops->refgain = (uint32_t) atoi(optarg)*10;
				break;
			case 'I':
					ops->refname=std::string(optarg);
				break;
			case 'C':
					ops->config_fname = std::string(optarg);
					ops->use_cfg = true;
			break;
			case 'A':
					ops->agc = true;
				break;
			case 'R':
					ops->no_header = true;
				break;
			case 'q':
					ops->quiet = true;
				break;
			case 'm':
					ops->rowmajor = true;
				break;
			case 'w':
					if (atoi(optarg)==8){
						ops->use32bitfloat = false;
					}
					else if(atoi(optarg)==32){
						ops->use32bitfloat = true;
					}
					else{
						cout << "Unknown wireformat: " << string(optarg) << endl;
					}
				break;
			case 'K':
					ops->krakensdr=true;
				break;
			case 'B':
					ops->krakenbiastee=true;
				break;
			default:
					usage();
				break;
		}
	}
	return 0;
};

int main(int argc, char **argv)
{

	int nfftqueue = 8;
	crefnoise * refnoise;

	cl_ops   ops = {"1000",false,2000000,uint32_t(1575.42e6),8,1<<14,4,100,100,false,"",false,false,false,false,false,false};
	ops.ndev = crtlsdr::get_device_count();
	cout << to_string(ops.ndev) << " devices found." << endl;
	parsecommandline(&ops,argc,argv);

	cout << "ops parsed\n"<< endl;
	if (ops.no_header){
		cout << "streaming in raw mode" << endl;
	}
	if (ops.krakensdr){
		ops.refname="1000";
	}

	if (crtlsdr::get_index_by_serial(ops.refname)<0){
		cout << "reference '"<< ops.refname <<"' not found, exiting" << endl;
		exit(1);
	}

	{
		barrier *startbarrier;

		//redirect stderr stream
		std::stringstream buffer;
		std::streambuf *old;

		exit_all=false;	

		if (ops.quiet){
			old = std::cerr.rdbuf(buffer.rdbuf()); 
			redir_stderr(true);
		}

		std::signal(SIGINT,signal_handler);
		std::signal(SIGUSR1,int_handler);

		lvector<csdrdevice *> v_devices;

		cout << "Using cdsp implementation & version " << cdsp::implementation() << " " << cdsp::version() << endl;
		
		//read device order from config file, if not given, build order by device index:
		std::vector<sdrdefs> vdefs;
		if (ops.use_cfg){
			cout << "Reading config " << ops.config_fname << endl;
			vdefs = cconfigfile::readconfig(ops.config_fname);
			ops.ndev = vdefs.size();
			ops.refname = cconfigfile::get_refname(vdefs);
		}
		else{
			uint32_t refindex = crtlsdr::get_index_by_serial(ops.refname);
			for(uint32_t n=0;n<ops.ndev;n++){
				sdrdefs d;
				d.devindex=n;
				d.serial  = crtlsdr::get_device_serial(n);
				if (n!=refindex)
					vdefs.push_back(d);
			}

		}
		//std::cout << "device vector has " << std::to_string(v_devices.size()) << std::endl;
		ctransport *transport=ctransport::init("tcp://*:5555",ops.no_header,ops.rowmajor,ops.use32bitfloat,vdefs.size()+1,ops.blocksize);

		crefsdr* ref_dev = new crefsdr(ops.asyncbufn,ops.blocksize,ops.fs,ops.fc);

		if (ref_dev->open(ops.refname)){
			cout << "could not open reference device, serial number:'" << ops.refname << "'" << endl;
			exit(1);
		}
		ref_dev->set_agc_mode(ops.agc);

		ref_dev->set_transport(transport);
		if (ops.krakenbiastee){
			cout << "Enable KrakenSDR bias tees (all for now)" << endl;
			ref_dev->set_bias_tee_state(0,true);
			ref_dev->set_bias_tee_state(1,true);
			ref_dev->set_bias_tee_state(2,true);
			ref_dev->set_bias_tee_state(3,true);
			ref_dev->set_bias_tee_state(4,true);
		}
		else{
			cout << "Disable KrakenSDR bias tees (all for now)" << endl;
			ref_dev->set_bias_tee_state(0,false);
			ref_dev->set_bias_tee_state(1,false);
			ref_dev->set_bias_tee_state(2,false);
			ref_dev->set_bias_tee_state(3,false);
			ref_dev->set_bias_tee_state(4,false);
		}

		if (ops.krakensdr){
			refnoise = new crefnoise(ref_dev);
		}
		else{
			refnoise = new crefnoise("/dev/ttyACM0");
		}

		for (auto n: vdefs){
			v_devices.push_back(new crtlsdr(ops.asyncbufn,ops.blocksize,ops.fs,ops.fc)); //this must be made std::unique_ptr or std::shared_ptr...
			if (v_devices.back()->open(n.serial)){
				delete v_devices.back(); //opening the signaldevice failed
				v_devices.pop_back();
			}
			else
			{
				v_devices.back()->set_agc_mode(ops.agc); //openin signaldevice succeeded
				v_devices.back()->set_transport(transport);
			}
		}

		refnoise->set_state(true);		
		
		startbarrier = new barrier(v_devices.size()+1);
		
		ref_dev->start(startbarrier);
		
		for (auto d : v_devices){
			d->start(startbarrier);
		}
		
		cconsole console(stderr_pipe,ref_dev, &v_devices,refnoise);
		
		console.start();

		ccoherent coherent(ref_dev,&v_devices, refnoise, nfftqueue);
		coherent.start();
				
		while(!exit_all){
			transport->send(); //main thread just waits on data and publishes when available.
		}

		coherent.request_exit(); 
		coherent.join();
		ref_dev->stop();
		ref_dev->request_exit();
		ref_dev->close();

		for (auto d : v_devices){
			d->stop();
			d->request_exit();
		}

		console.request_exit();

		for (auto d : v_devices)
			d->close();

		console.join();

		if (ops.quiet){
			std::cerr.rdbuf( old );
			close(stderr_pipe[0]);
		}
		
		delete startbarrier;
		delete ref_dev;
		ctransport::cleanup();
		delete refnoise;
	}
	return 1;
}