/* -*- c++ -*- */
/*
 * Copyright 2026 Mikko Laakso.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_AEGIR_AEGIRSDR_IMPL_H
#define INCLUDED_AEGIR_AEGIRSDR_IMPL_H

#include <gnuradio/Aegir/AegirSDR.h>
#include <zmq.h>
#include <vector>
#include <cstdint>

namespace gr {
  namespace Aegir {

    struct pkt_hdr {
        uint32_t globalseqn;
        uint32_t nchannels;
        uint32_t blocksize;
        uint32_t unused;
    };

    class AegirSDR_impl : public AegirSDR
    {
    private:
      int           d_nchannels;
      int           d_blocksize;     // complex samples per channel
      void*         d_ctx;
      void*         d_sock;          // ZMQ_SUB — data
      void*         d_ctrl_sock;     // ZMQ_DEALER — control
      std::vector<int8_t> d_buf;     // receive buffer
      size_t        d_pkt_size;      // expected packet byte length

    public:
      AegirSDR_impl(const std::string& address,
                    const std::string& ctrl_address,
                    int nchannels,
                    int blocksize,
                    int timeout_ms);
      ~AegirSDR_impl();

      bool start() override;
      bool stop() override;

      int work(int noutput_items,
               gr_vector_const_void_star& input_items,
               gr_vector_void_star& output_items) override;

      // Send a raw command string to the AegirSDR control socket.
      void send_command(const std::string& cmd);
    };

  } // namespace Aegir
} // namespace gr

#endif /* INCLUDED_AEGIR_AEGIRSDR_IMPL_H */
