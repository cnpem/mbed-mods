/*
 * Copyright (C) 2021-2025 CNPEM (cnpem.br)
 * Author: Guilherme Ricioli <guilherme.ricioli@lnls.br>
 */

#include "TCPStreamIntfModule.hpp"

TCPStreamIntfModule::TCPStreamIntfModule(
    EthernetInterface *p_net, uint16_t port, int timeout, uint32_t batch_size,
    rtos::Semaphore *p_batches, mbed::Callback<void()> ring_buff_reset_cb,
    mbed::Callback<uint32_t()> ring_buff_size_cb,
    mbed::Callback<uint32_t(int32_t*, uint32_t)> ring_buff_pop_cb,
    /* Module params */
    osPriority priority, uint32_t stack_size, unsigned char *stack_mem,
    const char *name) :
  Module(priority, stack_size, stack_mem, name), _p_net(p_net), _port(port),
  _timeout(timeout), _batch_size(batch_size), _p_batches(p_batches),
  _ring_buff_reset_cb(ring_buff_reset_cb),
  _ring_buff_size_cb(ring_buff_size_cb), _ring_buff_pop_cb(ring_buff_pop_cb) {}

  TCPStreamIntfModule::~TCPStreamIntfModule() {}

  void TCPStreamIntfModule::_task() {
    nsapi_error_t nsapi_status;
    TCPSocket server;

    server.set_blocking(true);

    nsapi_status = server.open(_p_net);

    debug("[TCPStreamIntfModule::_task] server.open rc: %d\n", nsapi_status);
    if(nsapi_status != NSAPI_ERROR_OK) {
      MBED_ERROR(
        MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ENOTRECOVERABLE),
        "[TCPStreamIntfModule::_task] server.open rc != NSAPI_ERROR_OK");
    }

    nsapi_status = server.bind(_port);

    debug("[TCPStreamIntfModule::_task] server.bind rc: %d\n", nsapi_status);
    if(nsapi_status != NSAPI_ERROR_OK) {
      MBED_ERROR(
        MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ENOTRECOVERABLE),
        "[TCPStreamIntfModule::_task] server.bind rc: != NSAPI_ERROR_OK");
    }

    nsapi_status = server.listen(1);

    debug("[TCPStreamIntfModule::_task] server.listen rc: %d\n", nsapi_status);
    if(nsapi_status != NSAPI_ERROR_OK) {
      MBED_ERROR(
        MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ENOTRECOVERABLE),
        "[TCPStreamIntfModule::_task] server.listen rc: != NSAPI_ERROR_OK");
    }

    while(true) {
      TCPSocket *client;
      bool is_client_accepted = false;

      server.set_blocking(false);

      _ring_buff_reset_cb();

      while(true) {
        uint32_t num_samples_to_pop;
        uint32_t num_samples_popped;

        _p_batches->acquire();

        if(!is_client_accepted) {
          client = server.accept(&nsapi_status);
          debug("[TCPStreamIntfModule::_task] server.accept rc: %d\n",
            nsapi_status);

          if(nsapi_status == NSAPI_ERROR_OK) {
            is_client_accepted = true;
          }
        }

        num_samples_to_pop = min(_ring_buff_size_cb(), _batch_size);

        if(num_samples_to_pop > 0) {
          int32_t data[_batch_size];

          num_samples_popped = _ring_buff_pop_cb(data, num_samples_to_pop);
          if(num_samples_popped != num_samples_to_pop) {
            MBED_ERROR(
              MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ENOTRECOVERABLE),
              "[TCPStreamIntfModule::_task] num_samples_popped != "
              "num_samples_to_pop");
          }

          if(is_client_accepted) {
            uint32_t num_bytes_to_be_sent;
            nsapi_size_or_error_t count;

            client->set_timeout(5000);

            num_bytes_to_be_sent = num_samples_popped*sizeof(uint32_t);

            count = client->send(data, num_bytes_to_be_sent);

            if(count < 0) {
              nsapi_status = client->close();

              if(nsapi_status != NSAPI_ERROR_OK) {
                MBED_ERROR(
                  MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ENOTRECOVERABLE),
                  "[TCPStreamIntfModule::_task] client->close rc != "
                  "NSAPI_ERROR_OK");
              }

              is_client_accepted = false;
            } else if(count != num_bytes_to_be_sent) {
              MBED_ERROR(
                MBED_MAKE_ERROR(MBED_MODULE_APPLICATION, ENOTRECOVERABLE),
                "[TCPStreamIntfModule::_task] partial data sent");
            }
          }
        }
      }
    }
  }
