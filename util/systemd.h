#pragma once

#include <optional>
#include <string>

#ifdef __linux__
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace spectatord {

#ifdef __linux__
// Reimplementation of systemd socket activation protocol without libsystemd dependency.
// This implements the stable socket activation protocol documented at:
// https://www.freedesktop.org/software/systemd/man/sd_listen_fds.html

constexpr int SD_LISTEN_FDS_START = 3;

// Check if a file descriptor is a socket of the specified type.
// Returns true if fd matches the criteria, false otherwise.
inline bool is_socket_type(int fd, int family, int type) {
  // Get socket domain (AF_UNIX, AF_INET, etc.)
  int sock_family;
  socklen_t len = sizeof(sock_family);
  if (getsockopt(fd, SOL_SOCKET, SO_DOMAIN, &sock_family, &len) < 0) {
    return false;
  }
  if (family != -1 && sock_family != family) {
    return false;
  }

  // Get socket type (SOCK_STREAM, SOCK_DGRAM, etc.)
  int sock_type;
  len = sizeof(sock_type);
  if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &sock_type, &len) < 0) {
    return false;
  }
  if (type != -1 && sock_type != type) {
    return false;
  }

  return true;
}

// Get the number of file descriptors passed by systemd.
// Returns the count, or 0 if not running under systemd socket activation.
inline int listen_fds() {
  const char* e = std::getenv("LISTEN_FDS");
  if (e == nullptr) {
    return 0;
  }

  char* endptr;
  long n = std::strtol(e, &endptr, 10);
  if (endptr == e || *endptr != '\0' || n < 0) {
    return 0;
  }

  // Unset the environment variable so child processes don't inherit it
  unsetenv("LISTEN_FDS");
  unsetenv("LISTEN_PID");

  return static_cast<int>(n);
}
#endif

// Check if systemd has passed us any socket file descriptors
// and return the FD for a UNIX datagram socket if found.
// Returns std::nullopt if not running under systemd socket activation.
inline auto get_systemd_socket() -> std::optional<int> {
#ifdef __linux__
  int n = listen_fds();
  if (n <= 0) {
    return std::nullopt;
  }

  // Look for a UNIX datagram socket
  for (int fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + n; ++fd) {
    if (is_socket_type(fd, AF_UNIX, SOCK_DGRAM)) {
      return fd;
    }
  }
#endif
  return std::nullopt;
}

} // namespace spectatord
