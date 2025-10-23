/*
 * Copyright (C) 2021-2025 CNPEM (cnpem.br)
 * Author: Guilherme Ricioli <guilherme.ricioli@lnls.br>
 */

#ifndef TCPSTREAMMODULE_HPP_
#define TCPSTREAMMODULE_HPP_

#include "EthernetInterface.h"

#include "Module.hpp"

/* TCPStreamIntfModule is a class providing a TCP binary interface for
 * streaming 32-bit integers.
 */
class TCPStreamIntfModule final :
  public Module {
    public:
      TCPStreamIntfModule(
          EthernetInterface *p_net, uint16_t port, int timeout,
          uint32_t batch_size, rtos::Semaphore *p_batches,
          mbed::Callback<void()> _ring_buff_reset_cb,
          mbed::Callback<uint32_t()> _ring_buff_size_cb,
          mbed::Callback<uint32_t(int32_t*, uint32_t)> _ring_buff_pop_cb,
          /* Module params */
          osPriority priority, uint32_t stack_size,
          unsigned char *stack_mem, const char *name);
      ~TCPStreamIntfModule();

    private:
      EthernetInterface *_p_net;
      uint16_t _port;
      int _timeout;
      char _separator;
      uint32_t _batch_size;
      rtos::Semaphore *_p_batches;
      mbed::Callback<void()> _ring_buff_reset_cb;
      mbed::Callback<uint32_t()> _ring_buff_size_cb;
      mbed::Callback<uint32_t(int32_t*, uint32_t)> _ring_buff_pop_cb;

      void _task();
  };

#endif /* TCPSTREAMMODULE_HPP_ */
