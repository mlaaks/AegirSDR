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
#include <fstream>
#include <vector>

using namespace std;

/*
The class cconfigfile. Crude config parser to support rudimentary
configuration files. Change implementation to some INI-parser
which supports Linux .conf files, even python will do.
*/

struct sdrdefs {
	uint32_t devindex;
	string   serial;
};

class cconfigfile{
public:
	static std::vector<sdrdefs> readconfig(string fname) {
		string ln;
		ifstream cfgfile(fname);
		std::vector<sdrdefs> v;

		while(getline(cfgfile,ln)){
			sdrdefs d;
			if (ln[0]=='#')
				continue;
			else{
				std::size_t st,end;
				std::string::size_type sz;
				string ids = ln.substr(0,2);
				
				if ((ids[0]=='R')||(ids[1]=='R'))
					d.devindex = 0;
				else
					d.devindex = std::stoi(ids,&sz);
				
				st = ln.find(":");
				st = ln.find("'",st+1);
				end= ln.find("'",st+1);
				d.serial = ln.substr(st+1,end-st-1);
				v.push_back(d);
				}
			
		}
		cfgfile.close();
		return v;
	}

	static string get_refname(std::vector<sdrdefs> vdefs){
		string ret;
		for(auto n : vdefs) {
				if (n.devindex==0){
					ret=n.serial;
					break;
				}
			}
		return ret;
	}

};
