# AegirSDR

KrakenSDR support for the old project coherent-rtlsdr.

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

