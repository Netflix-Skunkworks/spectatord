#pragma once

#include <optional>
#include <string>

#ifdef __linux__
#include <cstdlib>
#include <cstring>
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

// Send a notification message to systemd.
// This implements the systemd notification protocol documented at:
// https://www.freedesktop.org/software/systemd/man/sd_notify.html
//
// Common notification strings:
//   "READY=1" - Service startup is finished
//   "STATUS=..." - Status message
//   "WATCHDOG=1" - Watchdog keepalive
//   "STOPPING=1" - Service is stopping
//
// Returns true if the notification was sent successfully, false otherwise.
inline bool sd_notify(const std::string& state) {
#ifdef __linux__
  const char* notify_socket = std::getenv("NOTIFY_SOCKET");
  if (notify_socket == nullptr || notify_socket[0] == '\0') {
    return false;
  }

  // Create a UNIX datagram socket
  int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return false;
  }

  // Parse the socket address
  struct sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;

  size_t notify_socket_len = std::strlen(notify_socket);
  if (notify_socket_len >= sizeof(addr.sun_path)) {
    close(fd);
    return false;
  }

  // Handle abstract namespace sockets (starting with '@')
  if (notify_socket[0] == '@') {
    addr.sun_path[0] = '\0';
    std::strncpy(addr.sun_path + 1, notify_socket + 1, sizeof(addr.sun_path) - 2);
  } else {
    std::strncpy(addr.sun_path, notify_socket, sizeof(addr.sun_path) - 1);
  }

  // Send the notification
  ssize_t result = sendto(fd, state.c_str(), state.size(), MSG_NOSIGNAL,
                          reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

  close(fd);
  return result >= 0;
#else
  return false;
#endif
}

} // namespace spectatord
