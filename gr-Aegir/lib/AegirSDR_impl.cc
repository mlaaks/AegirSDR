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

namespace gr {
  namespace Aegir {

    AegirSDR::sptr
    AegirSDR::make(const std::string& address,
                   const std::string& ctrl_address,
                   int nchannels,
                   int blocksize,
                   int timeout_ms)
    {
      return gnuradio::make_block_sptr<AegirSDR_impl>(
          address, ctrl_address, nchannels, blocksize, timeout_ms);
    }

    AegirSDR_impl::AegirSDR_impl(const std::string& address,
                                 const std::string& ctrl_address,
                                 int nchannels,
                                 int blocksize,
                                 int timeout_ms)
      : gr::sync_block("AegirSDR",
                        gr::io_signature::make(0, 0, 0),
                        gr::io_signature::make(nchannels, nchannels, sizeof(gr_complex))),
        d_nchannels(nchannels),
        d_blocksize(blocksize),
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

      // work() is always called with a multiple of blocksize output slots
      set_output_multiple(blocksize);
    }

    AegirSDR_impl::~AegirSDR_impl()
    {
      if (d_sock)      zmq_close(d_sock);
      if (d_ctrl_sock) zmq_close(d_ctrl_sock);
      if (d_ctx)       zmq_ctx_destroy(d_ctx);
    }

    bool AegirSDR_impl::start()  { return true; }
    bool AegirSDR_impl::stop()   { return true; }

    void AegirSDR_impl::send_command(const std::string& cmd)
    {
      zmq_send(d_ctrl_sock, cmd.c_str(), cmd.size(), 0);
    }

    int AegirSDR_impl::work(int noutput_items,
                            gr_vector_const_void_star& /*input_items*/,
                            gr_vector_void_star& output_items)
    {
      // receive one packet (blocks up to timeout_ms)
      int rc = zmq_recv(d_sock, d_buf.data(), d_pkt_size, 0);
      if (rc <= 0)
        return 0;  // timeout or error — tell scheduler to call us again

      // validate header
      const pkt_hdr* hdr = reinterpret_cast<const pkt_hdr*>(d_buf.data());
      if ((int)hdr->nchannels != d_nchannels || (int)hdr->blocksize != d_blocksize) {
        std::cerr << "AegirSDR: packet header mismatch (N="
                  << hdr->nchannels << " L=" << hdr->blocksize << ")\n";
        return 0;
      }

      // data starts after hdr0 + N readcount words
      const int8_t* data = d_buf.data()
                         + sizeof(pkt_hdr)
                         + d_nchannels * sizeof(uint32_t);

      constexpr float scale = 1.0f / 128.0f;

      for (int ch = 0; ch < d_nchannels; ch++) {
        gr_complex* out       = static_cast<gr_complex*>(output_items[ch]);
        const int8_t* src     = data + ch * d_blocksize * 2;
        for (int i = 0; i < d_blocksize; i++) {
          out[i] = gr_complex(src[2*i] * scale, src[2*i+1] * scale);
        }
      }

      return d_blocksize;
    }

  } /* namespace Aegir */
} /* namespace gr */
