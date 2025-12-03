#pragma once

#include "handler.h"
#include <asio.hpp>

namespace spectatord {

class LocalServer {
 public:
  // Create a LocalServer that binds to a new socket at the given path
  // NOLINTNEXTLINE(google-runtime-references)
  LocalServer(asio::io_context& io_context, std::string_view path,
              handler_t handler);

  // Create a LocalServer using an existing socket file descriptor (systemd activation)
  // NOLINTNEXTLINE(google-runtime-references)
  LocalServer(asio::io_context& io_context, int socket_fd,
              handler_t handler);

  void Start();

 private:
  handler_t handler_;
  asio::local::datagram_protocol::socket socket_;
  std::array<char, 65536> recv_buffer_{};
  void start_local_receive();
};

}  // namespace spectatord
