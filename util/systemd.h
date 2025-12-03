#pragma once

#include <optional>
#include <string>

#if defined(__linux__) && defined(HAVE_SYSTEMD)
#include <systemd/sd-daemon.h>
#endif

namespace spectatord {

// Check if systemd has passed us any socket file descriptors
// and return the FD for a UNIX datagram socket if found.
// Returns std::nullopt if not running under systemd socket activation.
inline auto get_systemd_socket() -> std::optional<int> {
#if defined(__linux__) && defined(HAVE_SYSTEMD)
  // SD_LISTEN_FDS_START is the first file descriptor passed by systemd
  // File descriptors passed by systemd start at SD_LISTEN_FDS_START (typically 3)
  int n = sd_listen_fds(0);
  if (n < 0) {
    return std::nullopt;
  }

  // Look for a UNIX datagram socket
  for (int fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + n; ++fd) {
    // Check if this is a datagram socket
    if (sd_is_socket(fd, AF_UNIX, SOCK_DGRAM, -1) > 0) {
      return fd;
    }
  }
#endif
  return std::nullopt;
}

} // namespace spectatord
