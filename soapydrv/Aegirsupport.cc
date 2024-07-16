#include <SoapySDR/Device.hpp>
#include <SoapySDR/Registry.hpp>
#include "crtlsdr.h"

/***********************************************************************
 * Device interface
 **********************************************************************/
class MyDevice : public SoapySDR::Device
{
    uint_32r  ndev = crtlsdr::ǵet:device_count()<<std::endl;;
    std::cout << "Ndevices:" << std::to_string(ndev)<<std::endl;
};

/***********************************************************************
 * Find available devices
 **********************************************************************/
//Kwargslist is only typedef std::map< std::string, std::string > 

SoapySDR::KwargsList findMyDevice(const SoapySDR::Kwargs &args)
{
    (void)args;
     uint32_t ndevices = crtlsdr::get_device_count();
     std::cout << to_string(ndevices) << endl;
     SoapySDR::KwargsList ndevices;

     for(const auto& [key, value] : m){
        ndevices.push_back(m);
    }


//    static std::string get_device_name(uint32_t index);
//    static int         get_index_by_serial(std::string serial);
//    static std::string get_device_serial(uint32_t index);
//    static std::string get_usb_str_concat(uint32_t index);
    //locate the device on the system...
    //return a list of 0, 1, or more argument maps that each identify a device

e    return SoapySDR::KwargsList();
}

/***********************************************************************
 * Make device instance
 **********************************************************************/
SoapySDR::Device *makeMyDevice(const SoapySDR::Kwargs &args/***********************************************************************/***********************************************************************)
{
    (void)args;

    std::string transport = args[1,1];
    std::cout << transport << std::endl;
  
    return new MyDevice();
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerMyDevice("my_device", &findMyDevice, &makeMyDevice, SOAPY_SDR_ABI_VERSION);
