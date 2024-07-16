cmake_minimum_required (VERSION 3.5)
project (AegirSDR)

#to be removed? This was for RPI3 GPUFFT:
option(RASPBERRYPI "RPI" OFF)
if (RASPBERRYPI)
	add_definitions(-DRASPBERRYPI)
endif(RASPBERRYPI)

set(CMAKE_CXX_FLAGS "-O3") #-Wl,-Bdynamic

include_directories(${PROJECT_SOURCE_DIR}/include)
add_subdirectory(src)
add_subdirectory(soapydrv)
