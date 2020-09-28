#pragma once

#include <cstdint>
#include <optional>

namespace spectatord {
// get the maximum buffer size available on the system
int max_buffer_size(const char* proc_file = "/proc/sys/net/core/rmem_max");

struct udp_info_t {
  // total number of packets dropped between min_port and max_port
  int64_t num_dropped;
  // size of receive queue in bytes
  int64_t rx_queue_bytes;
};

std::optional<udp_info_t> udp_info(int port,
                                   const char* proc_file = "/proc/net/udp");

}  // namespace spectatord
