# AegirSDR

KrakenSDR support for the old project coherent-rtlsdr.

# Get dependencies on debian:
sudo apt update
sudo apt install build-essential git cmake libusb-1.0-0-dev libzmq3-dev

# Install the KrakenRF RTL-SDR kernel driver:

git clone https://github.com/krakenrf/librtlsdr
cd librtlsdr
sudo cp rtl-sdr.rules /etc/udev/rules.d/rtl-sdr.rules
mkdir build
cd build
cmake ../ -DINSTALL_UDEV_RULES=ON
make
sudo ln -s ~/librtlsdr/build/src/rtl_test /usr/local/bin/kraken_test

echo 'blacklist dvb_usb_rtl28xxu' | sudo tee --append /etc/modprobe.d/blacklist-dvb_usb_rtl28xxu.conf
