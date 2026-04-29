/* -*- c++ -*- */
/*
 * Copyright 2026 Mikko Laakso.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_AEGIR_AEGIRSDR_H
#define INCLUDED_AEGIR_AEGIRSDR_H

#include <gnuradio/Aegir/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
  namespace Aegir {

    /*!
     * \brief AegirSDR ZMQ source block.
     *
     * Receives coherent multi-channel IQ data from the AegirSDR daemon
     * over a ZMQ PUB/SUB socket and outputs N gr_complex streams.
     *
     * \ingroup Aegir
     */
    class AEGIR_API AegirSDR : virtual public gr::sync_block
    {
     public:
      typedef std::shared_ptr<AegirSDR> sptr;

      /*!
       * \brief Create an AegirSDR source block.
       *
       * \param address     ZMQ address of the AegirSDR data socket (e.g. "tcp://localhost:5555")
       * \param ctrl_address ZMQ address of the AegirSDR control socket (e.g. "tcp://localhost:5556")
       * \param nchannels   Number of receiver channels (outputs)
       * \param blocksize   Complex samples per channel per packet (L = ops.blocksize >> 1)
       * \param timeout_ms  ZMQ receive timeout in milliseconds (0 = block forever)
       */
      static sptr make(const std::string& address,
                       const std::string& ctrl_address,
                       int nchannels,
                       int blocksize,
                       int timeout_ms = 100);
    };

  } // namespace Aegir
} // namespace gr

#endif /* INCLUDED_AEGIR_AEGIRSDR_H */
