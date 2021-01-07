#pragma once

#include <atomic>
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include "absl/synchronization/mutex.h"

class http_server {
 public:
  http_server() noexcept;
  http_server(const http_server&) = delete;
  http_server(http_server&&) = delete;
  auto operator=(const http_server&) -> http_server& = delete;
  auto operator=(http_server &&) -> http_server& = delete;

  ~http_server();

  void set_read_sleep(std::chrono::milliseconds millis) {
    read_sleep_ = millis;
  }

  void set_accept_sleep(std::chrono::milliseconds millis) {
    accept_sleep_ = millis;
  }

  void set_sleep_number(int nr) { sleep_number_ = nr; }

  void start() noexcept;

  void stop() {
    is_done = true;
    accept_.join();
  }

  auto get_port() const -> int { return port_; };

  class Request {
   public:
    Request() : body_(nullptr) {}
    Request(std::string method, std::string path,
            std::map<std::string, std::string> headers, size_t size,
            std::unique_ptr<char[]>&& body)
        : method_(std::move(method)),
          path_(std::move(path)),
          headers_(std::move(headers)),
          size_(size),
          body_(std::move(body)) {}

    [[nodiscard]] auto size() const -> size_t { return size_; }
    [[nodiscard]] auto body() const -> const char* { return body_.get(); }
    [[nodiscard]] auto get_header(const std::string& name) const -> std::string;
    [[nodiscard]] auto method() const -> std::string { return method_; }
    [[nodiscard]] auto path() const -> std::string { return path_; }

   private:
    std::string method_;
    std::string path_;
    std::map<std::string, std::string> headers_{};
    size_t size_{0};
    std::unique_ptr<char[]> body_{};
  };

  auto get_requests() const -> const std::vector<Request>&;

 private:
  std::atomic<int> sockfd_{-1};
  std::atomic<int> port_{0};
  std::thread accept_{};

  std::atomic<int> sleep_number_{3};
  std::chrono::milliseconds accept_sleep_{0};
  std::chrono::milliseconds read_sleep_{0};

  std::atomic<bool> is_done{false};
  mutable absl::Mutex requests_mutex_{};
  std::vector<Request> requests_ GUARDED_BY(requests_mutex_);

  std::map<std::string, std::string> path_response_;

  void accept_request(int client);
  void accept_loop();
};
