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

  CurlHandle curl;
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

namespace {

struct MultiRequestContext {
  HttpRequest* request;
  std::string response_body;
  HttpHeaders response_headers;
  int http_code = -1;
  
  MultiRequestContext(HttpRequest* req) : request(req) {}
};

auto curl_multi_write_callback(char* contents, size_t size, size_t nmemb, void* userp) -> size_t {
  auto real_size = size * nmemb;
  auto* ctx = static_cast<MultiRequestContext*>(userp);
  ctx->response_body.append(contents, real_size);
  return real_size;
}

auto curl_multi_header_callback(char* contents, size_t size, size_t nmemb, void* userp) -> size_t {
  auto real_size = size * nmemb;
  auto end = contents + real_size;
  auto* ctx = static_cast<MultiRequestContext*>(userp);
  
  auto p = static_cast<char*>(memchr(contents, ':', real_size));
  if (p != nullptr && p + 2 < end) {
    std::string key{contents, p};
    std::string value{p + 2, end - 1};
    ctx->response_headers.emplace(std::make_pair(std::move(key), std::move(value)));
  }
  return real_size;
}

}  // namespace

HttpClientMulti::HttpClientMulti(Registry* registry, HttpClientConfig config)
    : registry_(registry), config_(std::move(config)) {
  multi_handle_ = curl_multi_init();
  if (!multi_handle_) {
    throw std::runtime_error("Failed to initialize curl multi handle");
  }
  
  auto* multi = static_cast<CURLM*>(multi_handle_);
  curl_multi_setopt(multi, CURLMOPT_MAX_HOST_CONNECTIONS, static_cast<long>(config_.max_host_connections));
  curl_multi_setopt(multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, static_cast<long>(config_.max_total_connections));
  
  // Pre-populate connection pool
  for (size_t i = 0; i < config_.max_total_connections; ++i) {
    auto* curl = curl_easy_init();
    if (curl) {
      connection_pool_.push(curl);
    }
  }
}

HttpClientMulti::~HttpClientMulti() {
  if (multi_handle_) {
    curl_multi_cleanup(static_cast<CURLM*>(multi_handle_));
  }
  
  // Clean up connection pool
  while (!connection_pool_.empty()) {
    curl_easy_cleanup(static_cast<CURL*>(connection_pool_.front()));
    connection_pool_.pop();
  }
}

auto HttpClientMulti::get_curl_handle() -> void* {
  std::lock_guard<std::mutex> lock(pool_mutex_);
  if (!connection_pool_.empty()) {
    auto* handle = connection_pool_.front();
    connection_pool_.pop();
    return handle;
  }
  
  // Pool is empty, create new handle
  return curl_easy_init();
}

void HttpClientMulti::return_curl_handle(void* handle) {
  if (!handle) return;
  
  auto* curl = static_cast<CURL*>(handle);
  curl_easy_reset(curl);
  
  std::lock_guard<std::mutex> lock(pool_mutex_);
  if (connection_pool_.size() < config_.max_total_connections) {
    connection_pool_.push(handle);
  } else {
    curl_easy_cleanup(curl);
  }
}

void HttpClientMulti::configure_curl_handle(void* handle, const HttpRequest& request) {
  auto* curl = static_cast<CURL*>(handle);
  
  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
  
  // Set headers
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request.headers->headers());
  
  // Set timeouts
  auto total_timeout = config_.connect_timeout + config_.read_timeout;
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, absl::ToInt64Milliseconds(total_timeout));
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, absl::ToInt64Milliseconds(config_.connect_timeout));
  
  // Set user agent
  auto user_agent = fmt::format("spectatord/{}", VERSION);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
  
  // Configure payload for POST requests
  if (request.method == "POST" && !request.payload.empty()) {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.payload.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.payload.size());
  }
  
  // Configure metatron if external
  if (config_.external_enabled) {
    curl_easy_setopt(curl, CURLOPT_SSLCERT, config_.cert_info.ssl_cert.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEY, config_.cert_info.ssl_key.c_str());
    curl_easy_setopt(curl, CURLOPT_CAPATH, nullptr);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
    curl_easy_setopt(curl, CURLOPT_CAINFO, config_.cert_info.ca_info.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, metatron::sslctx_metatron_verify);
    curl_easy_setopt(curl, CURLOPT_SSL_CTX_DATA, config_.cert_info.app_name.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
  }
  
  // Enable connection reuse
  if (config_.enable_connection_reuse) {
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L);
  }
  
  // Set verbose if requested
  if (config_.verbose_requests) {
    curl_easy_setopt(curl, CURLOPT_STDERR, stdout);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }
}

auto HttpClientMulti::PostAsync(const std::string& url, const char* content_type,
                                const void* payload, size_t size) -> std::future<HttpResponse> {
  auto headers = std::make_shared<CurlHeaders>();
  headers->append(content_type);
  
  if (config_.compress) {
    headers->append("Content-Encoding: gzip");
    
    auto compressed_size = compressBound(size) + kGzipHeaderSize;
    auto compressed_payload = std::unique_ptr<char[]>(new char[compressed_size]);
    auto compress_res = gzip_compress(compressed_payload.get(), &compressed_size, payload, size);
    
    if (compress_res != Z_OK) {
      auto logger = registry_->GetLogger();
      logger->info("Failed to compress payload: {}, while posting to {} - uncompressed size: {}",
                   compress_res, url, size);
      std::promise<HttpResponse> promise;
      HttpResponse err_response{-1, "", {}};
      promise.set_value(std::move(err_response));
      return promise.get_future();
    }
    
    auto request = std::make_unique<HttpRequest>(url, "POST", headers, 
                                                 compressed_payload.get(), compressed_size);
    auto future = request->promise.get_future();
    
    std::lock_guard<std::mutex> lock(requests_mutex_);
    active_requests_.push_back(std::move(request));
    return future;
  }
  
  // No compression
  auto request = std::make_unique<HttpRequest>(url, "POST", headers, payload, size);
  auto future = request->promise.get_future();
  
  std::lock_guard<std::mutex> lock(requests_mutex_);
  active_requests_.push_back(std::move(request));
  return future;
}

auto HttpClientMulti::PostAsync(const std::string& url, const char* content_type,
                                const CompressedResult& payload) -> std::future<HttpResponse> {
  auto headers = std::make_shared<CurlHeaders>();
  headers->append(content_type);
  headers->append("Content-Encoding: gzip");
  
  auto request = std::make_unique<HttpRequest>(url, "POST", headers, payload.data, payload.size);
  auto future = request->promise.get_future();
  
  std::lock_guard<std::mutex> lock(requests_mutex_);
  active_requests_.push_back(std::move(request));
  return future;
}

void HttpClientMulti::ProcessAll() {
  if (active_requests_.empty()) {
    return;
  }
  
  auto* multi = static_cast<CURLM*>(multi_handle_);
  std::vector<std::unique_ptr<MultiRequestContext>> contexts;
  
  // Setup all requests
  for (auto& request : active_requests_) {
    auto* curl = static_cast<CURL*>(get_curl_handle());
    if (!curl) continue;
    
    configure_curl_handle(curl, *request);
    
    auto ctx = std::make_unique<MultiRequestContext>(request.get());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_multi_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx.get());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_multi_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, ctx.get());
    curl_easy_setopt(curl, CURLOPT_PRIVATE, ctx.get());
    
    curl_multi_add_handle(multi, curl);
    contexts.push_back(std::move(ctx));
  }
  
  // Process all requests
  int running_handles;
  do {
    auto mc = curl_multi_perform(multi, &running_handles);
    if (mc != CURLM_OK) break;
    
    if (running_handles > 0) {
      curl_multi_wait(multi, nullptr, 0, 1000, nullptr);
    }
  } while (running_handles > 0);
  
  // Process completed requests
  CURLMsg* msg;
  int msgs_left;
  while ((msg = curl_multi_info_read(multi, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      auto* curl = msg->easy_handle;
      
      // Find the corresponding context
      MultiRequestContext* ctx = nullptr;
      for (auto& context : contexts) {
        void* curl_ctx = nullptr;
        curl_easy_getinfo(curl, CURLINFO_PRIVATE, &curl_ctx);
        if (curl_ctx == context.get()) {
          ctx = context.get();
          break;
        }
      }
      
      if (ctx) {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        ctx->http_code = static_cast<int>(http_code);
        
        HttpResponse response{ctx->http_code, std::move(ctx->response_body), std::move(ctx->response_headers)};
        ctx->request->promise.set_value(std::move(response));
      }
      
      curl_multi_remove_handle(multi, curl);
      return_curl_handle(curl);
    }
  }
  
  active_requests_.clear();
}

}  // namespace spectator
