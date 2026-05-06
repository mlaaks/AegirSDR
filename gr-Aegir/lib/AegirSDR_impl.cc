/* -*- c++ -*- */
/*
 * Copyright 2026 Mikko Laakso.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gnuradio/io_signature.h>
#include "AegirSDR_impl.h"
#include <stdexcept>
#include <iostream>
#include <cstdio>
#include <cstring>

namespace gr {
  namespace Aegir {

    AegirSDR::sptr
    AegirSDR::make(const std::string& address,
                   const std::string& ctrl_address,
                   int nchannels,
                   int blocksize,
                   double frequency,
                   double samplerate,
                   float gain,
                   int timeout_ms)
    {
      return gnuradio::make_block_sptr<AegirSDR_impl>(
          address, ctrl_address, nchannels, blocksize,
          frequency, samplerate, gain, timeout_ms);
    }

    AegirSDR_impl::AegirSDR_impl(const std::string& address,
                                 const std::string& ctrl_address,
                                 int nchannels,
                                 int blocksize,
                                 double frequency,
                                 double samplerate,
                                 float gain,
                                 int timeout_ms)
      : gr::sync_block("AegirSDR",
                        gr::io_signature::make(0, 0, 0),
                        gr::io_signature::make(nchannels, nchannels, sizeof(gr_complex))),
        d_nchannels(nchannels),
        d_blocksize(blocksize),
        d_frequency(frequency),
        d_samplerate(samplerate),
        d_gain(gain),
        d_ctx(nullptr),
        d_sock(nullptr),
        d_ctrl_sock(nullptr)
    {
      // packet layout: hdr0 (16B) + N*uint32 readcounts + N * blocksize * 2 int8
      d_pkt_size = sizeof(pkt_hdr)
                 + (size_t)nchannels * sizeof(uint32_t)
                 + (size_t)nchannels * blocksize * 2;
      d_buf.resize(d_pkt_size);

      d_ctx = zmq_ctx_new();
      if (!d_ctx)
        throw std::runtime_error("AegirSDR: zmq_ctx_new failed");

      d_sock = zmq_socket(d_ctx, ZMQ_SUB);
      if (!d_sock)
        throw std::runtime_error("AegirSDR: zmq_socket(SUB) failed");

      zmq_setsockopt(d_sock, ZMQ_SUBSCRIBE, "", 0);
      if (timeout_ms > 0)
        zmq_setsockopt(d_sock, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

      if (zmq_connect(d_sock, address.c_str()) != 0)
        throw std::runtime_error("AegirSDR: zmq_connect failed: " + address);

      d_ctrl_sock = zmq_socket(d_ctx, ZMQ_DEALER);
      if (!d_ctrl_sock)
        throw std::runtime_error("AegirSDR: zmq_socket(DEALER) failed");

      if (zmq_connect(d_ctrl_sock, ctrl_address.c_str()) != 0)
        throw std::runtime_error("AegirSDR: zmq_connect(ctrl) failed: " + ctrl_address);

      set_output_multiple(blocksize);
    }

    AegirSDR_impl::~AegirSDR_impl()
    {
      if (d_sock)      zmq_close(d_sock);
      if (d_ctrl_sock) zmq_close(d_ctrl_sock);
      if (d_ctx)       zmq_ctx_destroy(d_ctx);
    }

    void AegirSDR_impl::send_command(const std::string& cmd)
    {
      zmq_send(d_ctrl_sock, cmd.c_str(), cmd.size(), 0);
    }

    bool AegirSDR_impl::start()
    {
      set_frequency(d_frequency);
      set_samplerate(d_samplerate);
      set_gain(d_gain);
      return true;
    }

    bool AegirSDR_impl::stop() { return true; }

    void AegirSDR_impl::set_frequency(double freq)
    {
      d_frequency = freq;
      char cmd[64];
      std::snprintf(cmd, sizeof(cmd), "tuningfrequency %u", (uint32_t)freq);
      send_command(cmd);
    }

    void AegirSDR_impl::set_samplerate(double samplerate)
    {
      d_samplerate = samplerate;
      char cmd[64];
      std::snprintf(cmd, sizeof(cmd), "samplerate %u", (uint32_t)samplerate);
      send_command(cmd);
    }

    void AegirSDR_impl::set_gain(float gain)
    {
      d_gain = gain;
      char cmd[64];
      std::snprintf(cmd, sizeof(cmd), "tunergain %.1f", gain);
      send_command(cmd);
    }

    int AegirSDR_impl::work(int noutput_items,
                            gr_vector_const_void_star& /*input_items*/,
                            gr_vector_void_star& output_items)
    {
      int rc = zmq_recv(d_sock, d_buf.data(), d_pkt_size, 0);
      if (rc <= 0)
        return 0;

      const pkt_hdr* hdr = reinterpret_cast<const pkt_hdr*>(d_buf.data());
      if ((int)hdr->nchannels != d_nchannels || (int)hdr->blocksize != d_blocksize) {
        std::cerr << "AegirSDR: packet header mismatch (N="
                  << hdr->nchannels << " L=" << hdr->blocksize << ")\n";
        return 0;
      }

      const int8_t* data = d_buf.data()
                         + sizeof(pkt_hdr)
                         + d_nchannels * sizeof(uint32_t);

      constexpr float scale = 1.0f / 128.0f;

      for (int ch = 0; ch < d_nchannels; ch++) {
        gr_complex* out   = static_cast<gr_complex*>(output_items[ch]);
        const int8_t* src = data + ch * d_blocksize * 2;
        for (int i = 0; i < d_blocksize; i++)
          out[i] = gr_complex(src[2*i] * scale, src[2*i+1] * scale);
      }

      return d_blocksize;
    }

  } /* namespace Aegir */
} /* namespace gr */
