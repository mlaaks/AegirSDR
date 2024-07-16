########################################################################
# Project setup -- only needed if device support is a stand-alone build
# We recommend that the support module be built in-tree with the driver.
########################################################################
cmake_minimum_required(VERSION 3.5)
project(AegirSDR CXX)
enable_testing()

set(SOURCES ../src/csdrdevice.cc ../src/crtlsdr.cc)

#select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

########################################################################
# Header and library resources needed to communicate with the device.
# These may be found within the build tree or in an external project.
########################################################################
#set(MY_DEVICE_INCLUDE_DIRS "include")
#set(MY_DEVICE_LIBRARIES "src")

########################################################################
# build the module
########################################################################
find_package(SoapySDR CONFIG)

if (NOT SoapySDR_FOUND)
    message(WARNING "SoapySDR development files not found - skipping support")
    return()
endif ()

add_library(SoapyAegir SHARED ${SOURCES})
link_directories(${SOURCES})
target_link_libraries(AegirSDR)

install(TARGETS SoapyAegir
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION /usr/local/lib/SoapySDR/modules0.8)

#configure_file(
#    ${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
 #   ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
#@ONLY)

#add_custom_target(uninstall
#    ${CMAKE_COMMAND} -P #${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
#)