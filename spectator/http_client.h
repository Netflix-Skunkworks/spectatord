#pragma once

#include "absl/time/time.h"
#include "compressed_buffer.h"
#include "../metatron/metatron_config.h"
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace spectator {

class Registry;
class CurlHeaders;

struct HttpClientConfig {
  absl::Duration connect_timeout;
  absl::Duration read_timeout;
  bool compress;
  bool read_headers;
  bool read_body;
  bool verbose_requests;
  bool status_metrics_enabled;
  bool external_enabled;
  metatron::CertInfo cert_info;
};

using HttpHeaders = std::unordered_map<std::string, std::string>;
struct HttpResponse {
  int status;
  std::string raw_body;
  HttpHeaders headers;
};

class HttpClient {
 public:
  static constexpr const char* const kJsonType =
      "Content-Type: application/json";
  static constexpr const char* const kSmileJson =
      "Content-Type: application/x-jackson-smile";

  HttpClient(Registry* registry, HttpClientConfig config);

  auto Post(const std::string& url, const char* content_type,
            const void* payload, size_t size) const -> HttpResponse;

  auto Post(const std::string& url, const char* content_type,
            const CompressedResult& payload) const -> HttpResponse;

  auto Post(const std::string& url, const char* content_type,
            const std::string& payload) const -> HttpResponse {
    return Post(url, content_type, payload.c_str(), payload.length());
  };

  [[nodiscard]] auto Get(const std::string& url) const -> HttpResponse;
  [[nodiscard]] auto Get(const std::string& url,
                         const std::vector<std::string>& headers) const -> HttpResponse;

  [[nodiscard]] auto Put(const std::string& url,
                         const std::vector<std::string>& headers) const -> HttpResponse;

  static void GlobalInit() noexcept;
  static void GlobalShutdown() noexcept;

 private:
  Registry* registry_;
  HttpClientConfig config_;

  auto perform(const char* method, const std::string& url,
               std::shared_ptr<CurlHeaders> headers, const void* payload,
               size_t size, int attempt_number) const -> HttpResponse;

  auto method_header(const char* method, const std::string& url,
                     const std::vector<std::string>& headers) const -> HttpResponse;
};

}  // namespace spectator
