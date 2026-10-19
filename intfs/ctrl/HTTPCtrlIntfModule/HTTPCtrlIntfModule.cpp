/*
 * Copyright (C) 2025 CNPEM (cnpem.br)
 * Author: Vinicius Oliveira <vinicius.silva@lnls.br>
 */

#include "HTTPCtrlIntfModule.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <climits>

HTTPCtrlIntfModule::HTTPCtrlIntfModule(
    EthernetInterface *p_net,
    uint16_t port,
    int timeout,
    /* CtrlIntfModule params */
    const char terminator,
    const uint8_t max_cmd_len,
    mbed::Callback<bool(Kernel::Clock::duration_u32,
        CtrlIntfModuleMessage*, uint8_t)> try_put_for_cb,
    osPriority priority, uint32_t stack_size, unsigned char *stack_mem,
    const char *name,
    const char *web_page) :
  CtrlIntfModule(terminator, max_cmd_len, try_put_for_cb,
      priority, stack_size, stack_mem, name),
  _p_net(p_net), _port(port), _timeout(timeout), _web_page(web_page),
  _listening(false) {
  std::memset(_http_buff, 0, sizeof(_http_buff));
}

HTTPCtrlIntfModule::~HTTPCtrlIntfModule() {}

// This transport uses Content-Length.
// Complete the POST body even when headers and body arrive in separate recv()s.
namespace {
bool read_content_length(const char *request, const char *end,
                         size_t *content_length) {
  const char *line = std::strstr(request, "\r\n");
  if (!line || line >= end) return false;
  line += 2;
  static const char key[] = "content-length:";
  while (line < end) {
    const char *line_end = std::strstr(line, "\r\n");
    if (!line_end || line_end > end) return false;
    size_t key_len = sizeof(key) - 1;
    if (static_cast<size_t>(line_end - line) >= key_len) {
      bool found = true;
      for (size_t i = 0; i < key_len; ++i) {
        if (std::tolower(static_cast<unsigned char>(line[i])) != key[i]) {
          found = false;
          break;
        }
      }
      if (found) {
        const char *n = line + key_len;
        while (n < line_end && (*n == ' ' || *n == '\t')) ++n;
        if (n >= line_end || !std::isdigit(static_cast<unsigned char>(*n)))
          return false;
        size_t value = 0;
        while (n < line_end && std::isdigit(static_cast<unsigned char>(*n))) {
          const unsigned digit = static_cast<unsigned>(*n - '0');
          if (value > (SIZE_MAX - digit) / 10) return false;
          value = value * 10 + digit;
          ++n;
        }
        while (n < line_end && (*n == ' ' || *n == '\t')) ++n;
        if (n != line_end) return false;
        *content_length = value;
        return true;
      }
    }
    line = line_end + 2;
  }
  return false;
}
} // namespace

bool HTTPCtrlIntfModule::_read_http_request(TCPSocket *client, char *buff,
    size_t buff_len) {
  if (!client || !buff || buff_len < 8) return false;
  std::memset(buff, 0, buff_len);
  size_t total = 0;
  while (total < (buff_len - 1)) {
    nsapi_size_or_error_t ret = client->recv(buff + total, buff_len - 1 - total);
    if (ret <= 0) return false;
    total += static_cast<size_t>(ret);
    buff[total] = '\0';
    const char *headers_end = std::strstr(buff, "\r\n\r\n");
    if (!headers_end) continue;
    size_t header_bytes = static_cast<size_t>(headers_end - buff) + 4;
    if (std::strncmp(buff, "POST /cmd ", 10) != 0) return true;
    size_t body_bytes = 0;
    if (!read_content_length(buff, headers_end, &body_bytes) ||
        body_bytes == 0 || body_bytes > _max_cmd_len ||
        header_bytes + body_bytes >= buff_len) return false;
    if (total >= header_bytes + body_bytes) {
      buff[header_bytes + body_bytes] = '\0';
      return true;
    }
  }
  return false;
}

HTTPCtrlIntfModule::HttpRequestType HTTPCtrlIntfModule::_parse_http_request(
    const char *http_request, char *cmd_buff, size_t cmd_buff_len) {
  if (!http_request) {
    return HTTP_REQ_INVALID;
  }

  if (cmd_buff && cmd_buff_len > 0) {
    std::memset(cmd_buff, 0, cmd_buff_len);
  }

  if (std::strncmp(http_request, "GET / ", 6) == 0 ||
      std::strncmp(http_request, "GET /HTTP", 9) == 0 ||
      std::strncmp(http_request, "GET / HTTP/", 11) == 0) {
    return HTTP_REQ_GET_ROOT;
  }

  if (std::strncmp(http_request, "POST /cmd ", 10) != 0 &&
      std::strncmp(http_request, "POST /cmd HTTP/", 15) != 0) {
    return HTTP_REQ_INVALID;
  }

  const char *body = std::strstr(http_request, "\r\n\r\n");
  if (!body) {
    return HTTP_REQ_INVALID;
  }
  body += 4;

  while (*body == '\r' || *body == '\n' || *body == ' ') {
    body++;
  }

  if (*body == '\0' || !cmd_buff || cmd_buff_len == 0) {
    return HTTP_REQ_INVALID;
  }

  size_t len = strnlen(body, cmd_buff_len);
  while (len > 0 &&
      (body[len - 1] == '\r' || body[len - 1] == '\n' || body[len - 1] == ' ')) {
    len--;
  }

  if (len == 0 || len >= cmd_buff_len) {
    return HTTP_REQ_INVALID;
  }

  std::memcpy(cmd_buff, body, len);
  cmd_buff[len] = '\0';

  return HTTP_REQ_POST_CMD;
}

bool HTTPCtrlIntfModule::_send_all(TCPSocket *client, const char *data,
    size_t len) {
  if (!client || (!data && len > 0)) {
    return false;
  }

  size_t sent = 0;
  while (sent < len) {
    nsapi_size_or_error_t ret = client->send(data + sent, len - sent);
    if (ret <= 0) {
      return false;
    }
    sent += static_cast<size_t>(ret);
  }

  return true;
}

bool HTTPCtrlIntfModule::_send_http_response(TCPSocket *client, int status_code,
    const char *reason_phrase, const char *content_type, const char *body) {
  if (!client || !reason_phrase || !content_type) {
    return false;
  }

  if (!body) {
    body = "";
  }

  const size_t body_len = std::strlen(body);

  char header[256];
  int header_len = std::snprintf(header, sizeof(header),
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n"
      "\r\n",
      status_code, reason_phrase, content_type,
      static_cast<unsigned int>(body_len));

  if (header_len <= 0 || static_cast<size_t>(header_len) >= sizeof(header)) {
    return false;
  }

  if (!_send_all(client, header, static_cast<size_t>(header_len))) {
    return false;
  }

  return _send_all(client, body, body_len);
}

void HTTPCtrlIntfModule::_task() {
  char cmd_buff[_max_cmd_len + 1];

  while (true) {
    _listening.store(false);
    if (!_p_net ||
        _p_net->get_connection_status() != NSAPI_STATUS_GLOBAL_UP) {
      ThisThread::sleep_for(Kernel::Clock::duration_u32(1000));
      continue;
    }

    TCPSocket server;
    server.set_timeout(1000);

    nsapi_error_t status = server.open(_p_net);
    if (status != NSAPI_ERROR_OK) {
      ThisThread::sleep_for(Kernel::Clock::duration_u32(1000));
      continue;
    }

    status = server.bind(_port);
    if (status != NSAPI_ERROR_OK) {
      server.close();
      ThisThread::sleep_for(Kernel::Clock::duration_u32(1000));
      continue;
    }

    status = server.listen(1);
    if (status != NSAPI_ERROR_OK) {
      server.close();
      ThisThread::sleep_for(Kernel::Clock::duration_u32(1000));
      continue;
    }

    _listening.store(true);

    while (_p_net->get_connection_status() == NSAPI_STATUS_GLOBAL_UP) {
      nsapi_error_t accept_error = NSAPI_ERROR_OK;
      TCPSocket *client = server.accept(&accept_error);
      if (!client) {
        if (accept_error == NSAPI_ERROR_WOULD_BLOCK) continue;
        break; 
      }

      client->set_blocking(true);
      client->set_timeout(_timeout);

      if (!_read_http_request(client, _http_buff, sizeof(_http_buff))) {
        _send_http_response(client, 400, "Bad Request", "text/plain",
            "Bad Request");
        client->close();
        delete client;
        continue;
      }

      HttpRequestType req_type = _parse_http_request(_http_buff, cmd_buff,
          sizeof(cmd_buff));

      if (req_type == HTTP_REQ_GET_ROOT) {
        const char *page = (_web_page != nullptr) ? _web_page :
            "<!DOCTYPE html><html><body>No page configured.</body></html>";
        _send_http_response(client, 200, "OK", "text/html", page);
        client->close();
        delete client;
        continue;
      }

      if (req_type != HTTP_REQ_POST_CMD) {
        _send_http_response(client, 400, "Bad Request", "text/plain",
            "Bad Request");
        client->close();
        delete client;
        continue;
      }

      rtos::Semaphore ready(0, 1);
      CtrlIntfModuleMessage msg(cmd_buff, &ready);

      bool queued = _try_put_for_cb(
          rtos::Kernel::wait_for_u32_forever, &msg, 0);

      if (!queued) {
        _send_http_response(client, 503, "Service Unavailable", "text/plain",
            "Service Unavailable");
        client->close();
        delete client;
        continue;
      }

      ready.acquire();

      _send_http_response(client, 200, "OK", "text/plain", cmd_buff);
      client->close();
      delete client;
    }
    _listening.store(false);
    server.close();
    ThisThread::sleep_for(Kernel::Clock::duration_u32(1000));
  }
}
