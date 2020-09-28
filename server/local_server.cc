#include "local_server.h"
#include "logger.h"
#include "proc_utils.h"

namespace spectatord {

LocalServer::LocalServer(asio::io_context& io_context, std::string_view path,
                         handler_t handler)
    : handler_{std::move(handler)},
      socket_{io_context, asio::local::datagram_protocol::endpoint{path}} {}

void LocalServer::Start() { start_local_receive(); }

void LocalServer::start_local_receive() {
  socket_.async_receive(
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
          handler_(std::begin(recv_buffer_));
        }
        start_local_receive();
      });
}

}  // namespace spectatord
