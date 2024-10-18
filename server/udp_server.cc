#include "udp_server.h"
#include "util/logger.h"
#include "proc_utils.h"

using asio::ip::udp;

namespace spectatord {

// NOLINTNEXTLINE(google-runtime-references)
UdpServer::UdpServer(asio::io_context& io_context, bool ipv4_only, int port_number,
                     handler_t message_handler)
    : udp_socket_{io_context, udp::endpoint([&ipv4_only]() -> udp {
                    if (ipv4_only) return udp::v4(); else return udp::v6();
                  }() , port_number)},
      message_handler_(std::move(message_handler)) {
  asio::socket_base::receive_buffer_size option{max_buffer_size()};
  try {
    udp_socket_.set_option(option);
  } catch (const std::exception& e) {
    Logger()->warn("Unable to set max receive buffer size: {}", e.what());
  }
}

void UdpServer::start_udp_receive() {
  udp_socket_.async_receive(
      asio::buffer(recv_buffer_),
      [this](const std::error_code& err, size_t bytes_transferred) {
        if (bytes_transferred >= recv_buffer_.size()) {
          Logger()->error("too many bytes transferred: {} >= {}",
                          bytes_transferred, recv_buffer_.size());
        }

        if (err) {
          Logger()->error("Error receiving: {}: {}", err.value(),
                          err.message());
        } else if (bytes_transferred > 0) {
          recv_buffer_[bytes_transferred] = '\0';
          message_handler_(std::begin(recv_buffer_));
        }
        start_udp_receive();
      });
}
}  // namespace spectatord
