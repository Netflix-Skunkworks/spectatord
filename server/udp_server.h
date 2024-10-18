#pragma once

#include "handler.h"
#include <asio.hpp>

namespace spectatord {
class UdpServer {
 public:
  // NOLINTNEXTLINE(google-runtime-references)
  UdpServer(asio::io_context& io_context, bool ipv4_only, int port_number,
            handler_t message_handler);

  void Start() { start_udp_receive(); }

 private:
  asio::ip::udp::socket udp_socket_;
  std::array<char, 65536> recv_buffer_{};
  handler_t message_handler_;

  void start_udp_receive();
};
}  // namespace spectatord