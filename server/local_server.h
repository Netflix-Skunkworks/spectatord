#pragma once

#include "handler.h"
#include <asio.hpp>

namespace spectatord {

class LocalServer {
 public:
  // NOLINTNEXTLINE(google-runtime-references)
  LocalServer(asio::io_context& io_context, std::string_view path,
              handler_t handler);
  void Start();

 private:
  handler_t handler_;
  asio::local::datagram_protocol::socket socket_;
  std::array<char, 65536> recv_buffer_{};
  void start_local_receive();
};

}  // namespace spectatord
