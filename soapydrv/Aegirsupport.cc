// AegirSDR SoapySDR plugin
// Thin client that connects to a running AegirSDR daemon.
// Daemon publishes IQ data on port 5555 (ZMQ PUB).
// Control commands go to port 5556 (ZMQ ROUTER / console).
// MIT License - Copyright (c) 2024 Mikko Laakso

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>
#include <zmq.hpp>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <complex>
#include <algorithm>

// Must match ctransport.h hdr0
struct AegirHdr {
    uint32_t globalseqn;
    uint32_t N;     // number of channels
    uint32_t L;     // complex samples per channel (= blocksize >> 1)
    uint32_t unused;
};

static const char *DEFAULT_DATA_ADDR = "tcp://localhost:5555";
static const char *DEFAULT_CTRL_ADDR = "tcp://localhost:5556";

/***********************************************************************
 * Device implementation
 **********************************************************************/
class AegirDevice : public SoapySDR::Device
{
public:
    AegirDevice(const SoapySDR::Kwargs &args)
        : ctx_(1),
          data_sock_(ctx_, ZMQ_SUB),
          ctrl_sock_(ctx_, ZMQ_DEALER),
          nchannels_(0), blocksize_(0),
          fc_(0), fs_(0), gain_(0.0)
    {
        std::string data_addr = DEFAULT_DATA_ADDR;
        std::string ctrl_addr = DEFAULT_CTRL_ADDR;

        auto it = args.find("data_addr");
        if (it != args.end()) data_addr = it->second;
        it = args.find("ctrl_addr");
        if (it != args.end()) ctrl_addr = it->second;

        data_sock_.set(zmq::sockopt::linger, 0);
        ctrl_sock_.set(zmq::sockopt::linger, 0);
        data_sock_.set(zmq::sockopt::rcvtimeo, 2000);
        data_sock_.set(zmq::sockopt::subscribe, "");
        data_sock_.connect(data_addr);
        ctrl_sock_.connect(ctrl_addr);

        // Read first packet to discover N and L
        zmq::message_t probe;
        if (!data_sock_.recv(probe) || probe.size() < sizeof(AegirHdr))
            throw std::runtime_error("AegirSDR daemon not responding on " + data_addr);

        const AegirHdr *hdr = static_cast<const AegirHdr *>(probe.data());
        if (hdr->N == 0 || hdr->L == 0)
            throw std::runtime_error("AegirSDR daemon sent invalid header");

        nchannels_ = hdr->N;
        blocksize_ = hdr->L;  // complex samples per channel

        SoapySDR::log(SOAPY_SDR_INFO, "AegirSDR: connected, " +
                      std::to_string(nchannels_) + " channels, " +
                      std::to_string(blocksize_) + " samples/block");
    }

    // --- Identification ---
    std::string getDriverKey()   const override { return "AegirSDR"; }
    std::string getHardwareKey() const override { return "AegirSDR coherent receiver"; }

    // --- Channels ---
    size_t getNumChannels(const int dir) const override {
        return (dir == SOAPY_SDR_RX) ? nchannels_ : 0;
    }

    // --- Stream formats ---
    std::vector<std::string> getStreamFormats(const int dir, const size_t) const override {
        return {SOAPY_SDR_CF32, SOAPY_SDR_CS8};
    }

    std::string getNativeStreamFormat(const int dir, const size_t, double &fullScale) const override {
        fullScale = 128.0;
        return SOAPY_SDR_CS8;
    }

    // --- Streams ---
    SoapySDR::Stream *setupStream(const int dir,
                                   const std::string &format,
                                   const std::vector<size_t> &channels,
                                   const SoapySDR::Kwargs &) override
    {
        if (dir != SOAPY_SDR_RX)
            throw std::runtime_error("AegirSDR is RX-only");
        if (format != SOAPY_SDR_CF32 && format != SOAPY_SDR_CS8)
            throw std::runtime_error("Unsupported format: " + format);

        StreamHandle *h = new StreamHandle();
        h->format = format;
        if (channels.empty()) {
            h->channels.resize(nchannels_);
            for (size_t i = 0; i < nchannels_; ++i) h->channels[i] = i;
        } else {
            for (size_t ch : channels)
                if (ch >= nchannels_)
                    throw std::runtime_error("Invalid channel index: " + std::to_string(ch));
            h->channels = channels;
        }
        return reinterpret_cast<SoapySDR::Stream *>(h);
    }

    void closeStream(SoapySDR::Stream *stream) override {
        delete reinterpret_cast<StreamHandle *>(stream);
    }

    size_t getStreamMTU(SoapySDR::Stream *) const override { return blocksize_; }

    int activateStream(SoapySDR::Stream *, const int, const long long, const size_t) override {
        return 0;
    }

    int deactivateStream(SoapySDR::Stream *, const int, const long long) override {
        return 0;
    }

    int readStream(SoapySDR::Stream *stream,
                   void *const *buffs,
                   const size_t numElems,
                   int &flags,
                   long long &timeNs,
                   const long timeoutUs) override
    {
        StreamHandle *h = reinterpret_cast<StreamHandle *>(stream);

        int timeout_ms = static_cast<int>(timeoutUs / 1000);
        if (timeout_ms < 1) timeout_ms = 1;
        data_sock_.set(zmq::sockopt::rcvtimeo, timeout_ms);

        zmq::message_t msg;
        if (!data_sock_.recv(msg))
            return SOAPY_SDR_TIMEOUT;
        if (msg.size() < sizeof(AegirHdr))
            return SOAPY_SDR_OVERFLOW;

        const AegirHdr *hdr = static_cast<const AegirHdr *>(msg.data());
        const uint32_t N = hdr->N;
        const uint32_t L = hdr->L;  // complex samples per channel

        if (N == 0 || L == 0)
            return SOAPY_SDR_OVERFLOW;

        const size_t nSamples = std::min(static_cast<size_t>(L), numElems);

        // Packet layout (int8 mode, no header case excluded):
        //   [AegirHdr 16B][uint32 readcounts N*4B][channel 0: L*2 bytes][channel 1: L*2 bytes]...
        const uint8_t *base = static_cast<const uint8_t *>(msg.data())
                              + sizeof(AegirHdr) + N * sizeof(uint32_t);
        const size_t channelBytes = static_cast<size_t>(L) * 2;

        for (size_t i = 0; i < h->channels.size(); ++i) {
            const size_t ch = h->channels[i];
            if (ch >= N || buffs[i] == nullptr) continue;

            const int8_t *src = reinterpret_cast<const int8_t *>(base + ch * channelBytes);

            if (h->format == SOAPY_SDR_CF32) {
                auto *dst = static_cast<std::complex<float> *>(buffs[i]);
                constexpr float scale = 1.0f / 128.0f;
                for (size_t s = 0; s < nSamples; ++s)
                    dst[s] = scale * std::complex<float>(src[s * 2], src[s * 2 + 1]);
            } else {
                // CS8: raw int8 IQ pairs
                std::memcpy(buffs[i], src, nSamples * 2);
            }
        }

        if (nSamples < static_cast<size_t>(L))
            flags |= SOAPY_SDR_MORE_FRAGMENTS;

        return static_cast<int>(nSamples);
    }

    // --- Frequency ---
    void setFrequency(const int, const size_t,
                      const std::string &, const double freq,
                      const SoapySDR::Kwargs &) override
    {
        fc_ = static_cast<uint32_t>(freq);
        sendCommand("tuningfrequency " + std::to_string(fc_));
    }

    double getFrequency(const int, const size_t, const std::string &) const override {
        return static_cast<double>(fc_);
    }

    std::vector<std::string> listFrequencies(const int, const size_t) const override {
        return {"RF"};
    }

    SoapySDR::RangeList getFrequencyRange(const int, const size_t,
                                           const std::string &) const override {
        return {SoapySDR::Range(24e6, 1766e6)};
    }

    // --- Sample rate ---
    void setSampleRate(const int, const size_t, const double rate) override {
        fs_ = static_cast<uint32_t>(rate);
        sendCommand("samplerate " + std::to_string(fs_));
    }

    double getSampleRate(const int, const size_t) const override {
        return static_cast<double>(fs_);
    }

    std::vector<double> listSampleRates(const int, const size_t) const override {
        return {250000, 1024000, 1536000, 1792000, 1920000, 2048000,
                2160000, 2560000, 2880000, 3200000};
    }

    SoapySDR::RangeList getSampleRateRange(const int, const size_t) const override {
        // RTL-SDR valid ranges (with gap around 300–900 kHz)
        return {SoapySDR::Range(225001, 300000), SoapySDR::Range(900001, 3200000)};
    }

    // --- Gain ---
    void setGain(const int, const size_t, const double gain) override {
        gain_ = gain;
        sendCommand("tunergain " + std::to_string(static_cast<int>(gain)));
    }

    double getGain(const int, const size_t) const override { return gain_; }

    SoapySDR::Range getGainRange(const int, const size_t) const override {
        return SoapySDR::Range(0.0, 50.0);
    }

    std::vector<std::string> listGains(const int, const size_t) const override {
        return {"TUNER"};
    }

    // --- Antennas ---
    std::vector<std::string> listAntennas(const int, const size_t) const override {
        return {"RX"};
    }

    std::string getAntenna(const int, const size_t) const override { return "RX"; }

    // --- Bandwidth ---
    SoapySDR::RangeList getBandwidthRange(const int, const size_t) const override {
        return {SoapySDR::Range(0.0, 3200000.0)};
    }

private:
    struct StreamHandle {
        std::string format;
        std::vector<size_t> channels;
    };

    void sendCommand(const std::string &cmd) {
        ctrl_sock_.send(zmq::buffer(cmd), zmq::send_flags::none);
    }

    zmq::context_t ctx_;
    zmq::socket_t  data_sock_;
    zmq::socket_t  ctrl_sock_;
    uint32_t nchannels_;
    uint32_t blocksize_;
    uint32_t fc_;
    uint32_t fs_;
    double   gain_;
};

/***********************************************************************
 * Discovery
 **********************************************************************/
SoapySDR::KwargsList findAegirDevice(const SoapySDR::Kwargs &args)
{
    std::string data_addr = DEFAULT_DATA_ADDR;
    auto it = args.find("data_addr");
    if (it != args.end()) data_addr = it->second;

    try {
        zmq::context_t ctx(1);
        zmq::socket_t sock(ctx, ZMQ_SUB);
        sock.set(zmq::sockopt::linger, 0);
        sock.set(zmq::sockopt::rcvtimeo, 500);
        sock.set(zmq::sockopt::subscribe, "");
        sock.connect(data_addr);

        zmq::message_t msg;
        if (sock.recv(msg) && msg.size() >= sizeof(AegirHdr)) {
            const AegirHdr *hdr = static_cast<const AegirHdr *>(msg.data());
            SoapySDR::Kwargs result;
            result["driver"]    = "AegirSDR";
            result["data_addr"] = data_addr;
            result["ctrl_addr"] = DEFAULT_CTRL_ADDR;
            result["channels"]  = std::to_string(hdr->N);
            result["label"]     = "AegirSDR coherent receiver (" +
                                   std::to_string(hdr->N) + " ch)";
            return {result};
        }
    } catch (const std::exception &e) {
        SoapySDR::logf(SOAPY_SDR_DEBUG, "AegirSDR probe failed: %s", e.what());
    }

    return {};
}

SoapySDR::Device *makeAegirDevice(const SoapySDR::Kwargs &args)
{
    return new AegirDevice(args);
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerAegir(
    "AegirSDR", &findAegirDevice, &makeAegirDevice, SOAPY_SDR_ABI_VERSION);
