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

#include "console.h"

#include <iterator>
#include <zmq.hpp>
#include <iostream>
#include <sstream>
#include <string>

std::atomic<bool> exit_console;

std::atomic<bool> command_processed;

zmq::context_t 		ccontext(1);
zmq::socket_t  		csocket(ccontext,ZMQ_ROUTER);

lqueue<std::string> *commandqueue;


void localc(){
	std::string input;
	while(!exit_console){
		if (command_processed){
			std::getline(std::cin,input);
			commandqueue->push(std::string(input));
			command_processed=false;
		}
	}
	cout << "Local console exiting"<< endl;
}

void remotec(std::string address){
	zmq::message_t 		message;
	int timeout = 250;

	csocket.setsockopt(ZMQ_RCVTIMEO,&timeout,sizeof(int));
	csocket.bind(address);

	char *msg;
	size_t len;
	while(!exit_console){
		int nreceived = csocket.recv(&message);
		if (nreceived > 0) {
			msg = static_cast<char*>(message.data());
			len = message.size()/sizeof(char);
			cout << "remote command: "<< std::string(msg) << endl;
			commandqueue->push(std::string(msg));
		}
	}
	//csocket.unbind(address);
	csocket.close();
	//ccontext.close();
	cout << "Remote console exiting "<< endl;
}

void monitorc(std::string address){
	//socket.monitor("inproc://monitor.rep", ZMQ_EVENT_ALL)
	zmq_socket_monitor((void*)csocket, "inproc://monitor-client", ZMQ_EVENT_ALL);

	 zmq_event_t *event;
	 char *addr;
	 zmq::message_t message;
	 zmq::message_t addrmsg;
	 int64_t more;
	 size_t moresize;

	// zmq::socket_t msocket(context,ZMQ_PAIR);
	 csocket.connect("inproc://monitor-client");

	while(!exit_console){
	 	csocket.recv(&message);
	 	//socket.getsockopt(ZMQ_RCVMORE,&more,&moresize); //get more? these messages should arrive in pairs.

	 	if(more){ //second part ist the address..
	 		csocket.recv(&message);
	 		addr = static_cast<char *>(message.data());
	 	}

	 	event = static_cast<zmq_event_t*>(message.data());
	 	
	 	switch(event->event){
	 		case ZMQ_EVENT_LISTENING:
	 			cout << "Control socket listening at: " << std::string(addr) <<endl;
	 		break;
	 		case ZMQ_EVENT_ACCEPTED:
	 			cout << "Accepted " << std::string(addr) << endl;
	 		break;
	 		default:
	 		break;
	 	}
	}
}


std::string fmtfloat(std::string fmt,float f){
	char b[16];
	snprintf(b,16,fmt.data(),f);
	return string(b);
}

cconsole::cconsole(int pipe[2],crefsdr* refdev_,lvector<csdrdevice*> *devvec_,crefnoise * refnoise_): thread(){
	refdev =refdev_;
	devices=devvec_;
	stderr_pipe = pipe;
	refnoise = refnoise_;
	do_exit=false;
	exit_console=false;
}

cconsole::~cconsole(){
	if (thread.joinable()) thread.join();
}

void cconsole::join(){
	thread.join();
}

void cconsole::start(){
	thread = std::thread(cconsole::consolethreadf,this);
}

void cconsole::request_exit(){
	do_exit=true;
}


int cconsole::cmdfs(std::string opt){

	if (opt.compare("samplerate")==0)
		cout << fmtfloat("%4.6f",refdev->get_samplerate()/1e6) << " MHz" << endl;
	else
		try{
			float fs_  = stof(opt);
			uint32_t fs= uint32_t(fs_);

			refdev->set_samplerate(fs);
			for (auto *d : *devices){
				d->set_samplerate(fs);
				d->set_synchronized(false);
			}
		}
		catch (const std::exception& e){
			cout << "invalid argument: " << e.what() << "("<< opt <<")" << endl;
		}
    return 0;
}
int cconsole::cmdretune(std::string opt){
	
	//cout << "options string is " << opt << endl; //REMOVE

	if (opt.compare("fcenter")==0)
		cout << fmtfloat("%4.3f",refdev->get_fcenter()/1e6) << " MHz"<<endl;
	else
		try{
			float fcenter = stof(opt);
			uint32_t fc   = uint32_t(fcenter);

			printf("requested tuning freq: %d",fc); //REMOVE

			if ((fc>=1000000) && (fc<1800000000)){
				cout << "retuning to " << fmtfloat("%4.3f",fcenter/1e6) << " MHz" << endl;
				refdev->set_fcenter(fc);
				for (auto *d : *devices){
					d->set_fcenter(fc);
				}
			}
		}
		catch (const std::invalid_argument& ia){
			cout << "invalid argument: " << ia.what() <<"("<< opt <<")"<< endl;
		}
    return 0;
}

int cconsole::cmdlist(std::string opt){
	bool all = (opt.compare("all")==0);

	for (uint32_t n=0; n < crtlsdr::get_device_count();++n){
		string devs = crtlsdr::get_usb_str_concat(n);
		string name = crtlsdr::get_device_serial(n);
		string capt ="";

		for(lvector<csdrdevice*>::iterator d = devices->begin(); d != devices->end(); ++d)
		{
			if (name.compare((*d)->get_devname())==0){
				capt = string("\t * capturing, signal channel #"+ to_string(std::distance(devices->begin(),d)));
				if(!all)
					cout << devs << capt << endl;
			}
		}
		if (all)
			cout << devs << capt << endl;
	}
	return 0;				
}


//Flagged for deletion. No need to add or delete devices at runtime
int cconsole::cmddel(std::string opt){
	for(lvector<csdrdevice*>::iterator d = devices->begin(); d != devices->end(); ++d){
		if (opt.compare((*d)->get_devname()) == 0){
			

			//devices->lock();				 //this is too slow, interrupts streaming, but currently only way to avoid the deadlock situation....
			cout << "stopping " << opt << endl;
			(*d)->stop();
			cout << "exiting " << opt << endl;
			(*d)->request_exit();
			cout << "closing " << opt << endl;
			(*d)->close();
			cout << "deleting pointer " << opt << endl;
			csdrdevice *tmp = (*d);
			devices->erase(d);
			delete tmp; // delete after removal, since controller may be using the device until it's removed from container.
			return 0;
		}
	}
	cout << "'"<<opt<<"' not running"<<endl;
	return -1;
}

//Flagged for deletion. No need to add or delete devices at runtime
int cconsole::cmdadd(std::string opt){

	for (auto *d : *devices)
		if (opt.compare(d->get_devname())==0){
			cout <<"'" << opt << "' already capturing" << endl;
			return -1;
		}

	devices->push_back(new crtlsdr(refdev->get_asyncbufn(),refdev->get_blocksize(),refdev->get_samplerate(),refdev->get_fcenter()));
	
	if (devices->back()->open(opt)){
		delete devices->back();
		devices->pop_back();
		cout<<"could not open '" << opt <<"'" << endl;
	}
	startbarrier = new barrier(2);
	devices->back()->start(startbarrier);
	startbarrier->wait();

	delete startbarrier;
    
    return 0;
}
int cconsole::cmdrequest(std::string opt){

	if (opt.compare("re")==0){
		cout << "enable reference noise"<<endl;
		refnoise->set_state(true);
	}
	else if (opt.compare("rd")==0){
		refnoise->set_state(false);
		cout << "disable reference noise"<<endl;
	}
	else if (opt.compare("lag")==0){
		for (auto *d : *devices)
			d->requestfftblocking();
	}
	else if (opt.compare("sync")==0){
		for (auto *d : *devices){
			d->set_synchronized(false);
		}
	}
	else if (opt.compare("biasteeon")==0){
		cout << "setting bias tee on, all channels" << endl;
		for(uint32_t channel=0;channel<5;channel++){
			refdev->set_bias_tee_state(channel,true);
		}
	}
	else if (opt.compare("biasteeoff")==0){
		cout << "setting bias tee off, all channels" << endl;
		for(uint32_t channel=0;channel<5;channel++){
			refdev->set_bias_tee_state(channel,false);
		}
	}
	
	return 0;
}

int cconsole::cmdphase(std::string opt){
	for(auto *d : *devices){
		d->convtofloat();
		std::complex<float> p = d->est_phasecorrect(refdev->get_samplepointer()+(refdev->get_blocksize()>>1));
		cout<< to_string(int(180.0f/M_PI*atan2f(p.imag(),p.real()))) << "\t";
		//cout<< to_string(atan2f(p.imag(),p.real())) << "\t";

		phistory_t ph;
		ph.t = std::chrono::high_resolution_clock::now();
		ph.p = p;
		phistory.push_back(ph);
	}
	cout << endl;
    return 0;
}

int cconsole::cmdstatus(std::string opt){
	int numsynch=0;
	for (auto *d : *devices){
		if(d->get_synchronized()) numsynch++;
	}
	cout << to_string(numsynch) <<" / " << to_string(devices->size()) << " synchronized" << endl;
	if (refnoise->isenabled())
		cout << "Reference noise ENABLED." << endl;
	else 
		cout << "Reference noise DISABLED." << endl;

	int cc=0;
	for (auto *d : *devices){
		cout << d->get_devname() << ":" << fmtfloat("%+4.3f",d->get_lagp()->lag) << ":" << fmtfloat("%4.0f",d->get_lagp()->mag) << "\t"; 
		if ((++cc % 4)==0) //4 devs / line
			cout << endl;
	}
	cout << endl;
    return 0;
}

int cconsole::cmdtunergain(std::string opt){
	
	try{
		float gain = stof(opt);
		int tunergain = int(gain)*10; //specified in tenths of db in RTLSDR_API

		cout << "Setting tuner gains to " << to_string(tunergain) << endl;

		for (auto *d : *devices){
			d->set_tuner_gain(tunergain);
		}
		refdev->set_tuner_gain(tunergain);
	}
	catch (const std::invalid_argument& ia){
			cout << "invalid argument: " << ia.what() <<"("<< opt <<")"<< endl;
	}
	return 0;
}

int cconsole::parsecmd(std::string inputln){
	int cmd;
	size_t wspace = inputln.find(" ");
	try{
			cmd=command_codes.at(inputln.substr(0,wspace));
		} catch (const std::out_of_range& err){
			cmd = nop;
		}
	return cmd;
}

std::string cconsole::getoptionstr(std::string inputln){
	size_t wspace = inputln.find(" ");
	return inputln.substr(wspace+1,inputln.npos);
}

void cconsole::consolethreadf(cconsole *ctx)
{
	char buf[1024];
	std::string inputln;
	std::string options;
	int cmd;

	commandqueue = new lqueue<std::string>;
	std::thread local(localc);
	
	std::thread remote(remotec,"tcp://*:5556");
	command_processed=true;

	while(!ctx->do_exit){
		cmd=nop;
		inputln = commandqueue->pop();

		cmd = ctx->parsecmd(inputln);
		options = ctx->getoptionstr(inputln);
		if (!ctx->do_exit) //reorder the loop to get rid of this check...
		switch (cmd){
			case help:
				cout <<"help"<<endl;
			break;
			case samplerate:
				ctx->cmdfs(options);
			break;
			case status:
				ctx->cmdstatus(options);
			break;
			case list:
				ctx->cmdlist(options);
			break;
			case logs:
				{
				if (read(ctx->stderr_pipe[0], buf, sizeof(buf))>0)
					cout << std::string(buf) << endl;
				}
			break;
			case nop:
			break;
			case tuningfrequency:
				ctx->cmdretune(options);
			break;
			case request:
				ctx->cmdrequest(options);
			break;
			case phase:
				ctx->cmdphase(options);
			break;
			case tunergain:
				cout << "tunergain" << endl;
				ctx->cmdtunergain(options);
			break;
			case quit:
				ctx->request_exit();
				exit_console = true;
			break;
			default:
			break;

		}
		command_processed=true;
	}
	
	if (local.joinable()) local.join();
	if (remote.joinable()) remote.join();
	
	delete commandqueue;

	cout << "console thread exiting"<<endl;
	std::raise(SIGINT); //preferably this should modify exit_all in main.cc
}
