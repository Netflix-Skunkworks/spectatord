#include "http_client.h"
#include "gzip.h"
#include "log_entry.h"
#include "version.h"

#include <algorithm>
#include <utility>
#include <curl/curl.h>

namespace spectator {

class CurlHeaders {
 public:
  CurlHeaders() = default;
  ~CurlHeaders() { curl_slist_free_all(list_); }
  CurlHeaders(const CurlHeaders&) = delete;
  CurlHeaders(CurlHeaders&&) = delete;
  auto operator=(const CurlHeaders&) -> CurlHeaders& = delete;
  auto operator=(CurlHeaders &&) -> CurlHeaders& = delete;
  void append(const std::string& string) {
    list_ = curl_slist_append(list_, string.c_str());
  }
  auto headers() -> curl_slist* { return list_; }

 private:
  curl_slist* list_{nullptr};
};

namespace {

auto curl_ignore_output_fun(char* /*unused*/, size_t size, size_t nmemb, void * /*unused*/)
    -> size_t {
  return size * nmemb;
}

auto curl_capture_output_fun(char* contents, size_t size, size_t nmemb, void* userp) -> size_t {
  auto real_size = size * nmemb;
  auto* resp = static_cast<std::string*>(userp);
  resp->append(contents, real_size);
  return real_size;
}

auto curl_capture_headers_fun(char* contents, size_t size, size_t nmemb, void* userp) -> size_t {
  auto real_size = size * nmemb;
  auto end = contents + real_size;
  auto* headers = static_cast<HttpHeaders*>(userp);
  // see if it's a proper header and not HTTP/xx or the final \n
  auto p = static_cast<char*>(memchr(contents, ':', real_size));
  if (p != nullptr && p + 2 < end) {
    std::string key{contents, p};
    std::string value{p + 2, end - 1};  // drop last lf
    headers->emplace(std::make_pair(std::move(key), std::move(value)));
  }
  return real_size;
}

class CurlHandle {
 public:
  CurlHandle() noexcept : handle_{curl_easy_init()} {
    auto user_agent = fmt::format("spectatord/{}", VERSION);
    curl_easy_setopt(handle_, CURLOPT_USERAGENT, user_agent.c_str());

    // Enable connection reuse for thread-local handles
    curl_easy_setopt(handle_, CURLOPT_TCP_KEEPALIVE, 1L);
    // Cache up to 2 connections per handle. Each thread only talks to one aggregator
    // endpoint, so 1 is sufficient for steady state. We use 2 to handle the edge case
    // where an old connection is closing while a new one is being established.
    curl_easy_setopt(handle_, CURLOPT_MAXCONNECTS, 2L);
    curl_easy_setopt(handle_, CURLOPT_FORBID_REUSE, 0L);  // Allow connection reuse
  }

  CurlHandle(const CurlHandle&) = delete;

  auto operator=(const CurlHandle&) -> CurlHandle& = delete;

  CurlHandle(CurlHandle&& other) = delete;

  auto operator=(CurlHandle&& other) -> CurlHandle& = delete;

  ~CurlHandle() {
    // nullptr is handled by curl
    curl_easy_cleanup(handle_);
  }

  [[nodiscard]] auto handle() const noexcept -> CURL* { return handle_; }

  [[nodiscard]] auto perform() const -> CURLcode { return curl_easy_perform(handle()); }

  auto set_opt(CURLoption option, const void* param) const -> CURLcode {
    return curl_easy_setopt(handle(), option, param);
  }

  [[nodiscard]] auto status_code() const -> int {
    // curl requires this to be a long
    long http_code = 400;
    curl_easy_getinfo(handle(), CURLINFO_RESPONSE_CODE, &http_code);
    return static_cast<int>(http_code);
  }

  [[nodiscard]] auto response() const -> std::string { return response_; }

  void move_response(std::string* out) { *out = std::move(response_); }

  [[nodiscard]] auto headers() const -> HttpHeaders { return resp_headers_; }

  void move_headers(HttpHeaders* out) { *out = std::move(resp_headers_); }

  void set_url(const std::string& url) const { set_opt(CURLOPT_URL, url.c_str()); }

  void set_headers(std::shared_ptr<CurlHeaders> headers) {
    headers_ = std::move(headers);
    set_opt(CURLOPT_HTTPHEADER, headers_->headers());
  }

  void set_connect_timeout(absl::Duration connect_timeout) {
    auto millis = absl::ToInt64Milliseconds(connect_timeout);
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS, millis);
  }

  void set_timeout(absl::Duration total_timeout) {
    auto millis = absl::ToInt64Milliseconds(total_timeout);
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS, millis);
  }

  void post_payload(const void* payload, size_t size) {
    payload_ = payload;
    curl_easy_setopt(handle_, CURLOPT_POST, 1L);
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, payload_);
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE, size);
  }

  void custom_request(const char* method) {
    curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, method);
  }

  void configure_metatron(const HttpClientConfig& config) {
    // provide metatron client certificate during handshake
    curl_easy_setopt(handle_, CURLOPT_SSLCERT, config.cert_info.ssl_cert.c_str());
    curl_easy_setopt(handle_, CURLOPT_SSLKEY, config.cert_info.ssl_key.c_str());
    // disable use of system CAs
    curl_easy_setopt(handle_, CURLOPT_CAPATH, NULL);
    // perform full trust verification of server based on metatron CAs
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER, 1);
    curl_easy_setopt(handle_, CURLOPT_CAINFO, config.cert_info.ca_info.c_str());
    // install Metatron verifier and provide application name to check
    curl_easy_setopt(handle_, CURLOPT_SSL_CTX_FUNCTION, metatron::sslctx_metatron_verify);
    curl_easy_setopt(handle_, CURLOPT_SSL_CTX_DATA, config.cert_info.app_name.c_str());
    // disable hostname verification, SANs not present in metatron certs
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYHOST, 0);
    // save any extended error message
    curl_easy_setopt(handle_, CURLOPT_ERRORBUFFER, errbuf_);
  }

  void ignore_output() {
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, curl_ignore_output_fun);
  }

  void capture_output() {
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, curl_capture_output_fun);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, static_cast<void*>(&response_));
  }

  void capture_headers() {
    curl_easy_setopt(handle_, CURLOPT_HEADERDATA, static_cast<void*>(&resp_headers_));
    curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, curl_capture_headers_fun);
  }

  void trace_requests() {
    // we log to stdout - might need to make it configurable
    // in the future. For now let's keep it simple
    curl_easy_setopt(handle_, CURLOPT_STDERR, stdout);
    curl_easy_setopt(handle_, CURLOPT_VERBOSE, 1L);
  }

  char* get_errbuf() {
    return errbuf_;
  }

  // Clear per-request state when reusing handle across requests.
  // curl_easy_reset() clears all options but preserves the connection cache.
  void clear_for_reuse() {
    response_.clear();
    resp_headers_.clear();
    headers_.reset();
    payload_ = nullptr;
    std::memset(errbuf_, 0, CURL_ERROR_SIZE);

    // Reset all options while preserving connections
    curl_easy_reset(handle_);

    // Re-apply persistent settings
    auto user_agent = fmt::format("spectatord/{}", VERSION);
    curl_easy_setopt(handle_, CURLOPT_USERAGENT, user_agent.c_str());
    curl_easy_setopt(handle_, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(handle_, CURLOPT_MAXCONNECTS, 2L);
    curl_easy_setopt(handle_, CURLOPT_FORBID_REUSE, 0L);
  }

 private:
  CURL* handle_;
  std::shared_ptr<CurlHeaders> headers_;
  const void* payload_ = nullptr;
  std::string response_;
  HttpHeaders resp_headers_;
  char errbuf_[CURL_ERROR_SIZE]{};
};

}  // namespace

HttpClient::HttpClient(Registry* registry, HttpClientConfig config)
    : registry_(registry), config_{std::move(config)} {}

auto HttpClient::Get(const std::string& url) const -> HttpResponse {
  return perform("GET", url, std::make_shared<CurlHeaders>(), nullptr, 0u, 0);
}

auto HttpClient::Get(const std::string& url, const std::vector<std::string>& headers)
    const -> HttpResponse {
  return method_header("GET", url, headers);
}

auto HttpClient::Put(const std::string& url, const std::vector<std::string>& headers)
    const -> HttpResponse {
  return method_header("PUT", url, headers);
}

auto HttpClient::method_header(const char* method, const std::string& url,
                               const std::vector<std::string>& headers)
    const -> HttpResponse {
  auto curl_headers = std::make_shared<CurlHeaders>();
  for (const auto& h : headers) {
    curl_headers->append(h);
  }
  return perform(method, url, std::move(curl_headers), nullptr, 0u, 0);
}

inline auto is_retryable_error(int http_code) -> bool {
  return http_code == 429 || (http_code / 100) == 5;
}

auto HttpClient::perform(const char* method, const std::string& url,
                         std::shared_ptr<CurlHeaders> headers,
                         const void* payload, size_t size,
                         int attempt_number) const -> HttpResponse {
  LogEntry entry{registry_, method, url};

  // Use thread-local handle to enable connection reuse across requests.
  // Each thread maintains its own handle with cached connections, significantly
  // reducing connection churn and TLS handshake overhead for periodic publishing.
  thread_local CurlHandle curl;
  curl.clear_for_reuse();

  auto total_timeout = config_.connect_timeout + config_.read_timeout;
  curl.set_timeout(total_timeout);
  curl.set_connect_timeout(config_.connect_timeout);

  auto logger = registry_->GetLogger();
  curl.set_url(url);
  curl.set_headers(headers);

  if (strcmp("POST", method) == 0) {
    curl.post_payload(payload, size);
  } else if (strcmp("GET", method) != 0) {
    curl.custom_request(method);
  }
  if (config_.external_enabled) {
    curl.configure_metatron(config_);
  }
  if (config_.read_body) {
    curl.capture_output();
  } else {
    curl.ignore_output();
  }
  if (config_.read_headers) {
    curl.capture_headers();
  }
  if (config_.verbose_requests) {
    curl.trace_requests();
  }

  auto curl_res = curl.perform();
  int http_code;

  if (curl_res != CURLE_OK) {
    auto errbuff = curl.get_errbuf();
    if (errbuff[0] == '\0') {
      logger->info("Failed to {} {}: {}",
                    method, url, curl_easy_strerror(curl_res));
    } else {
      logger->info("Failed to {} {}: {} (errbuf={})",
                    method, url, curl_easy_strerror(curl_res), errbuff);
    }

    switch (curl_res) {
      case CURLE_COULDNT_CONNECT:
        entry.set_error("connection_error");
        break;
      case CURLE_OPERATION_TIMEDOUT:
        entry.set_error("timeout");
        break;
      default:
        entry.set_error("unknown");
    }

    auto elapsed = absl::Now() - entry.start();
    // retry connect timeouts if possible, not read timeouts
    logger->info(
        "HTTP timeout to {}: {}ms elapsed - connect_to={} read_to={}", url,
        absl::ToInt64Milliseconds(elapsed),
        absl::ToInt64Milliseconds(config_.connect_timeout),
        absl::ToInt64Milliseconds(total_timeout - config_.connect_timeout));

    if (elapsed < total_timeout && attempt_number < 2) {
      entry.set_attempt(attempt_number, false);
      entry.log(config_.status_metrics_enabled);
      return perform(method, url, std::move(headers), payload, size, attempt_number + 1);
    }

    http_code = -1;
    entry.set_status_code(-1);
  } else {
    http_code = curl.status_code();
    entry.set_status_code(http_code);

    if (http_code / 100 == 2) {
      entry.set_success();
    } else {
      entry.set_error("http_error");
    }

    if (is_retryable_error(http_code) && attempt_number < 2) {
      logger->info("Got a retryable http code from {}: {} (attempt {})", url,
                   http_code, attempt_number);
      entry.set_attempt(attempt_number, false);
      entry.log(config_.status_metrics_enabled);
      auto sleep_ms = uint32_t(200) << attempt_number;  // 200, 400ms
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
      return perform(method, url, std::move(headers), payload, size, attempt_number + 1);
    }
    logger->debug("{} {} - status code: {}", method, url, http_code);
  }
  entry.set_attempt(attempt_number, true);
  entry.log(config_.status_metrics_enabled);

  std::string resp;
  curl.move_response(&resp);

  HttpHeaders resp_headers;
  curl.move_headers(&resp_headers);
  return HttpResponse{http_code, std::move(resp), std::move(resp_headers)};
}

static constexpr const char* const kGzipEncoding = "Content-Encoding: gzip";

auto HttpClient::Post(const std::string& url, const char* content_type,
                      const CompressedResult& payload) const -> HttpResponse {
  auto headers = std::make_shared<CurlHeaders>();
  headers->append(content_type);
  headers->append(kGzipEncoding);

  return perform("POST", url, std::move(headers), payload.data, payload.size, 0);
}

auto HttpClient::Post(const std::string& url, const char* content_type,
                      const void* payload, size_t size) const -> HttpResponse {
  auto logger = registry_->GetLogger();
  auto headers = std::make_shared<CurlHeaders>();
  headers->append(content_type);

  if (config_.compress) {
    headers->append(kGzipEncoding);
    auto compressed_size = compressBound(size) + kGzipHeaderSize;
    auto compressed_payload = std::unique_ptr<char[]>(new char[compressed_size]);
    auto compress_res = gzip_compress(compressed_payload.get(),
                                      &compressed_size, payload, size);

    if (compress_res != Z_OK) {
      logger->info(
          "Failed to compress payload: {}, while posting to {} - uncompressed size: {}",
          compress_res, url, size);
      HttpResponse err{};
      err.status = -1;
      return err;
    }

    return perform("POST", url, std::move(headers), compressed_payload.get(), compressed_size, 0);
  }

  // no compression
  return perform("POST", url, std::move(headers), payload, size, 0);
}

void HttpClient::GlobalInit() noexcept {
  static bool init = false;
  if (init) {
    return;
  }

  init = true;
  curl_global_init(CURL_GLOBAL_ALL);
}

void HttpClient::GlobalShutdown() noexcept {
  static bool shutdown = false;
  if (shutdown) {
    return;
  }
  shutdown = true;
  curl_global_cleanup();
}

}  // namespace spectator
