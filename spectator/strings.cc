#include "strings.h"
#include <algorithm>

namespace spectator {

std::string PathFromUrl(std::string_view url) noexcept {
  if (url.empty()) {
    return "/";
  }

  auto proto_end = std::find(url.begin(), url.end(), ':');
  if (proto_end == url.end()) {
    return std::string{url};  // no protocol, assume just a path
  }

  // length of the string after http: - should be at least 3 for ://
  if ((url.end() - proto_end) < 3) {
    return std::string{url};
  }
  proto_end += 3;  // skip over ://

  auto path_begin = std::find(proto_end, url.end(), '/');
  if (path_begin == url.end()) {
    return "/";
  }

  auto query_begin = std::find(path_begin, url.end(), '?');
  return std::string{path_begin, query_begin};
}

}  // namespace spectator
