#include <fstream>
#include <iostream>
#include "yaml-cpp/yaml.h"
#include <cassert>
#include <map>
#include <vector>
#include "common.h"

using namespace std;
vector<string> readyaml(cl_ops *ops,std::string fname)
{
   YAML::Node node = YAML::LoadFile(fname.data());
   YAML::Node nparameters = node["parameters"];
   YAML::Node nodechild = nparameters["id"];
   cout << nparameters <<endl;
   
   int nsamples = nparameters[0]["default"].as<int>();
   cout << to_string(nsamples)<<endl;
  
   double fcenter = nparameters[1]["default"].as<double>();
   cout << to_string(fcenter)<<endl;
   int32_t samplerate = nparameters[2]["default"].as<int32_t>();
   cout << to_string(samplerate)<<endl;

   string ref_dev = nparameters[3]["referencedevice"].as<string>();
   
   std::vector<string> signaldevices;
   signaldevices.push_back(ref_dev);
   signaldevices.push_back(nparameters[3]["signaldevice1"].as<string>());
   signaldevices.push_back(nparameters[3]["signaldevice2"].as<string>());
   signaldevices.push_back(nparameters[3]["signaldevice3"].as<string>());
   signaldevices.push_back(nparameters[3]["signaldevice4"].as<string>());
   
   ops->blocksize=nsamples*2; //I&Q parts interleaved,hence *2
   ops->fc = fcenter;
   ops->refname = ref_dev;

   return signaldevices;
}
