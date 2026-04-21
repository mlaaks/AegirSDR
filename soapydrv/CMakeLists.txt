cmake_minimum_required(VERSION 3.5)
project(SoapyAegir CXX)

find_package(SoapySDR CONFIG)
if(NOT SoapySDR_FOUND)
    message(WARNING "SoapySDR development files not found - skipping AegirSDR plugin")
    return()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# SOAPY_SDR_MODULE_UTIL handles includes, link libs, and install path
SOAPY_SDR_MODULE_UTIL(
    TARGET SoapyAegir
    SOURCES Aegirsupport.cc
)

target_include_directories(SoapyAegir PRIVATE ${ZMQ_INCLUDE_DIRS})
target_link_libraries(SoapyAegir PRIVATE ${ZMQ_LIBRARIES})
