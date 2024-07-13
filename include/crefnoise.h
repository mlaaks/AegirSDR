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

#ifndef CREFNOISEH
#define CREFNOISEH

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
#endif
