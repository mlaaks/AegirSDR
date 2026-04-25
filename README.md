![AegirSDR](https://github.com/mlaaks/AegirSDR/blob/main/graphics/logo.png)

Extended support for similar SDR platforms as in our previous _coherent-rtlsdr_ project. This includes e.g. KrakenSDR.
One goal is to add support for SoapySDR API. By agreement of all the authors, we changed the licensing from GPL3 to the
MIT license. The code repository is now public, but not yet advertised anywhere. Still have things to fix, many things have
already been fixed - the code now exits cleanly.

The config files have now been changed to yaml syntax. Thus, the dependencies also include yaml-cpp, a thing that I regret.
Probably I will get rid of that dependency and code a simple parser myself.

RUNNING ON A VM IS DISCOURAGED AS IT WILL NOT SYNCHRONIZE.

Installation instructions are as follows:

# Get dependencies on debian:

```
sudo apt update
sudo apt install build-essential git cmake libusb-1.0-0-dev libzmq3-dev libfftw3-dev libvolk-dev libreadline-dev
```

# Install the KrakenRF RTL-SDR kernel driver:

```
git clone https://github.com/krakenrf/librtlsdr
cd librtlsdr
sudo cp rtl-sdr.rules /etc/udev/rules.d/rtl-sdr.rules
mkdir build
cd build
cmake ../ -DINSTALL_UDEV_RULES=ON
make
sudo ln -s ~/librtlsdr/build/src/rtl_test /usr/local/bin/kraken_test
echo 'blacklist dvb_usb_rtl28xxu' | sudo tee --append /etc/modprobe.d/blacklist-dvb_usb_rtl28xxu.conf
```

# Compiling AegirSDR
```
git clone https://github.com/mlaaks/AegirSDR
cd AegirSDR
mkdir build
cd build
cmake ..
make
```
The executable should now be located in build/src/AegirSDR
Currently, to run on the KrakenSDR from the above folder:

```
./AegirSDR -K
```
This will start the coherent receiver software (-K is for Kraken). Default tuning frequency: 1575.42MHz,
default samplerate: 2.0MHz. The receiver opens ZMQ sockets on ports 5555 (IQ sampledata), 5556 control
port for remote 'console' and 5557 (debug port for observing channel phasecorrection factors)

# Compiling AegirSDR for Raspbian

Tested on the Desktop version:
(Release date: March 15th 2024,System: 64-bit,Kernel version: 6.6,Debian version: 12 (bookworm))

First install dependencies using apt-get:

```
sudo apt update
sudo apt install build-essential git cmake libusb-1.0-0-dev libzmq3-dev libfftw3-dev libreadline-dev
```

VOLK (Vector Optimized Library of Kernels) must be built from source:
dependencies for VOLK:

```
sudo apt install python3-mako
```
get VOLK source:

```
git clone https://github.com/gnuradio/volk
```

NOTE, the following file has to be edited (before they fix it, but as of now 5.6.2024):
volk/cmake/Toolchains/arm_cortex_a72_hardfp_native.cmake
remove the CMAKE_CXX_FLAGS -mfpu=neon-fp-armv8 -mfloat-abi=hard

otherwise CMake will produce errors, floating point is now enforced on 64-bit ARM.


If everything worked so far, VOLK should now build and install with:

```
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchains/arm_cortex_a72_hardfp_native.cmake ..
make
```

