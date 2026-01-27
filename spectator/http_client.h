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
#include <queue>
#include <mutex>
#include <future>
#include <functional>

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
  size_t max_host_connections = 4;
  size_t max_total_connections = 16;
  bool enable_connection_reuse = true;
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

struct HttpRequest {
  std::string url;
  std::string method;
  std::shared_ptr<CurlHeaders> headers;
  std::vector<char> payload;
  std::promise<HttpResponse> promise;
  
  HttpRequest(std::string url, std::string method,
              std::shared_ptr<CurlHeaders> headers, 
              const void* payload_data, size_t payload_size)
      : url(std::move(url)), method(std::move(method)), headers(std::move(headers)) {
    if (payload_data != nullptr && payload_size > 0) {
      payload.assign(static_cast<const char*>(payload_data),
                     static_cast<const char*>(payload_data) + payload_size);
    }
  }
};

class HttpClientMulti {
 public:
  HttpClientMulti(Registry* registry, HttpClientConfig config);
  ~HttpClientMulti();

  HttpClientMulti(const HttpClientMulti&) = delete;
  HttpClientMulti(HttpClientMulti&&) = delete;
  auto operator=(const HttpClientMulti&) -> HttpClientMulti& = delete;
  auto operator=(HttpClientMulti&&) -> HttpClientMulti& = delete;

  auto PostAsync(const std::string& url, const char* content_type,
                 const void* payload, size_t size) -> std::future<HttpResponse>;

  auto PostAsync(const std::string& url, const char* content_type,
                 const CompressedResult& payload) -> std::future<HttpResponse>;

  void ProcessAll();

 private:
  Registry* registry_;
  HttpClientConfig config_;
  void* multi_handle_;  // CURLM*
  std::queue<void*> connection_pool_;  // Queue of CURL* handles
  std::mutex pool_mutex_;
  std::vector<std::unique_ptr<HttpRequest>> active_requests_;
  std::mutex requests_mutex_;

  auto get_curl_handle() -> void*;
  void return_curl_handle(void* handle);
  void configure_curl_handle(void* handle, const HttpRequest& request);
};

}  // namespace spectator
