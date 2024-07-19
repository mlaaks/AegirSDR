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

#include <iostream>
#include <fstream>
#include "crtlsdr.h"

/*
    TODO: Edit the code to crefnoise.cc file from header file
*/

/*
The crefnoise class: controls the noise source. In my original 
implementation, this was done using a microcontroller GPIO driving
a high-side power switch. In KrakenSDR, they use rtl-sdr GPIOs.
*/


class crefnoise{
private:
	ofstream fp;
	bool enabled;
	crefsdr* ref_dev;
	
public:
	void set_state(bool s){
		enabled = s;

		if (ref_dev==NULL){
			if (s)
				fp<<"x"; //enable char
			else
				fp<<"o"; //disable char (right now any other than 'x')
			
			fp.flush(); //may not send single char immediately unless we flush buffers
		}
		else{
			//cout<<"Trying to enable reference noise " << ref_dev << endl;
			ref_dev->set_reference_noise_state(s);
		}
	};

	bool isenabled(){
		return enabled;
	}

	crefnoise(std::string dev){
		ref_dev=NULL;
		fp.open(dev.data());
		if (!fp.good())
			cout<<"Error opening reference noise control '" << dev << endl;
		else{
			enabled = true;
			fp<<"x";
			fp.flush();
		}

	};

	crefnoise(crefsdr* d){
		ref_dev=d;
	}

	~crefnoise(){
		fp.close();
	};
};
