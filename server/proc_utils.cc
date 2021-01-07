#include "proc_utils.h"
#include "util/files.h"
#include <fcntl.h>
#include <unistd.h>
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"

namespace spectatord {
static constexpr int kDefault = 16 * 1024 * 1024;
auto max_buffer_size(const char* proc_file) -> int {
  static int result = 0;
  static const char* last_proc_file = nullptr;

  // compare the pointers only which are used only for testing
  if (result > 0 && last_proc_file == proc_file) {
    return result;
  }
  last_proc_file = proc_file;

  int fd = open(proc_file, 0);
  if (fd < 0) {
    result = kDefault;
  } else {
    result = kDefault;
    char buf[4096];
    int n = read(fd, buf, sizeof buf);
    if (n > 0) {
      buf[n] = '\0';
      if (!absl::SimpleAtoi(buf, &result)) {
        // unable to parse
        result = kDefault;
      }
    }
    close(fd);
  }
  return result;
}

auto udp_info(int port, const char* proc_file) -> std::optional<udp_info_t> {
  StdIoFile udp{proc_file};
  if (udp == nullptr) return {};

  udp_info_t res{};
  char line[4096];
  // discard header
  if (fgets(line, sizeof line, udp) == nullptr) return {};

  while (fgets(line, sizeof line, udp) != nullptr) {
    std::vector<std::string> fields =
        absl::StrSplit(line, absl::ByAnyChar(" :\n"), absl::SkipEmpty());
    if (fields.size() < 17) continue;
    auto cur_port = strtol(fields[2].c_str(), nullptr, 16);
    if (port != cur_port) continue;

    res.rx_queue_bytes = strtoul(fields[7].c_str(), nullptr, 16);
    res.num_dropped = strtoul(fields[16].c_str(), nullptr, 10);
    return res;
  }
  return {};
}

}  // namespace spectatord
