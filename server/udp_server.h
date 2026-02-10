#pragma once

#include "handler.h"
#include <asio.hpp>

namespace spectatord {
class UdpServer {
 public:
  // Create a UdpServer that binds to a new socket on the given port
  // NOLINTNEXTLINE(google-runtime-references)
  UdpServer(asio::io_context& io_context, bool ipv4_only, int port_number,
            handler_t message_handler);

  // Create a UdpServer using an existing socket file descriptor (systemd activation)
  // NOLINTNEXTLINE(google-runtime-references)
  UdpServer(asio::io_context& io_context, int socket_fd, bool is_ipv6,
            handler_t message_handler);

  void Start() { start_udp_receive(); }

 private:
  asio::ip::udp::socket udp_socket_;
  std::array<char, 65536> recv_buffer_{};
  handler_t message_handler_;

  void start_udp_receive();
};
}  // namespace spectatord