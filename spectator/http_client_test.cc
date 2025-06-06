#include <atomic>
#include <gtest/gtest.h>
#include <zlib.h>

#include <fmt/ostream.h>
#include "../spectator/gzip.h"
#include "../spectator/http_client.h"
#include "../spectator/registry.h"
#include "../spectator/strings.h"
#include "http_server.h"
#include "test_utils.h"
#include "percentile_bucket_tags.inc"

namespace {

using spectator::Config;
using spectator::GetConfiguration;
using spectator::gzip_uncompress;
using spectator::HttpClient;
using spectator::HttpClientConfig;
using spectator::HttpResponse;
using spectator::Id;
using spectator::intern_str;
using spectator::Registry;
using spectator::Tags;
using spectator::Timer;
using spectatord::Logger;

const Timer* find_timer(Registry* registry, const std::string& name,
                        const std::string& status_code) {
  auto meters = registry->Timers();
  auto status_code_ref = intern_str(status_code);
  for (const auto& m : meters) {
    auto meter_name = m->MeterId().Name().Get();
    if (meter_name == name) {
      auto t = m->MeterId().GetTags().at(intern_str("http.status"));
      if (t == status_code_ref) {
        return m;
      }
    }
  }
  return nullptr;
}

class TestRegistry : public Registry {
 public:
  explicit TestRegistry(std::unique_ptr<Config> config)
      : Registry(std::move(config), spectatord::Logger()) {}
};

HttpClientConfig get_cfg(int read_to, int connect_to) {
  auto cert_info = metatron::CertInfo{"ssl_cert", "ssl_key", "ca_info", "app_name"};
  return HttpClientConfig{absl::Milliseconds(connect_to), absl::Milliseconds(read_to),
                          true, true, true, false, true, false, cert_info};
}

TEST(HttpTest, Post) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = spectatord::Logger();
  logger->info("Server started on port {}", port);

  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, get_cfg(100, 100)};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  client.Post(url, "Content-type: application/json", post_data.c_str(),
              post_data.length());

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  server.stop();

  auto timer_for_req = find_timer(&registry, "ipc.client.call", "200");
  ASSERT_TRUE(timer_for_req != nullptr);
  auto expected_tags = Tags{
    {"http.method", "POST"},
    {"http.status", "200"},
    {"ipc.attempt.final", "true"},
    {"ipc.endpoint", "/foo"},
    {"ipc.result", "success"},
    {"ipc.status", "success"},
    {"nf.process", "spectatord"},
  };

  auto& actual_tags = timer_for_req->MeterId().GetTags();
  auto attempt = actual_tags.at(intern_str("ipc.attempt"));
  expected_tags.add(intern_str("ipc.attempt"), attempt);

  EXPECT_EQ(expected_tags, actual_tags);

  const auto& requests = server.get_requests();
  EXPECT_EQ(requests.size(), 1);

  const auto& r = requests[0];
  EXPECT_EQ(r.method(), "POST");
  EXPECT_EQ(r.path(), "/foo");
  EXPECT_EQ(r.get_header("Content-Encoding"), "gzip");
  EXPECT_EQ(r.get_header("Content-Type"), "application/json");

  const auto src = r.body();
  const auto src_len = r.size();
  char dest[8192];
  size_t dest_len = sizeof dest;
  auto res = gzip_uncompress(dest, &dest_len, src, src_len);
  ASSERT_EQ(res, Z_OK);

  std::string body_str{dest, dest_len};
  EXPECT_EQ(post_data, body_str);
}

TEST(HttpTest, PostUncompressed) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = spectatord::Logger();
  logger->info("Server started on port {}", port);

  TestRegistry registry{GetConfiguration()};
  auto cfg = get_cfg(100, 100);
  cfg.compress = false;
  HttpClient client{&registry, cfg};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto resp = client.Post(url, "Content-type: application/json",
                          post_data.c_str(), post_data.length());

  EXPECT_EQ(resp.status, 200);
  EXPECT_EQ(resp.raw_body, std::string("OK\n"));

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  server.stop();

  auto timer_for_req = find_timer(&registry, "ipc.client.call", "200");
  ASSERT_TRUE(timer_for_req != nullptr);
  auto expected_tags = Tags{
    {"http.method", "POST"},
    {"http.status", "200"},
    {"ipc.attempt", "initial"},
    {"ipc.attempt.final", "true"},
    {"ipc.endpoint", "/foo"},
    {"ipc.result", "success"},
    {"ipc.status", "success"},
    {"nf.process", "spectatord"},
  };

  const auto& actual_tags = timer_for_req->MeterId().GetTags();
  EXPECT_EQ(expected_tags, actual_tags);

  const auto& requests = server.get_requests();
  EXPECT_EQ(requests.size(), 1);

  const auto& r = requests[0];
  EXPECT_EQ(r.method(), "POST");
  EXPECT_EQ(r.path(), "/foo");
  EXPECT_EQ(r.get_header("Content-Type"), "application/json");

  const auto src = r.body();
  const auto src_len = r.size();

  std::string body_str{src, src_len};
  EXPECT_EQ(post_data, body_str);
}

TEST(HttpTest, Timeout) {
  http_server server;
  server.set_read_sleep(std::chrono::milliseconds(100));
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  TestRegistry registry{GetConfiguration()};
  auto logger = registry.GetLogger();
  logger->info("Server started on port {}", port);

  HttpClient client{&registry, get_cfg(10, 10)};
  auto url = fmt::format("http://localhost:{}/foo", port);
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto response = client.Post(url, "Content-type: application/json",
                              post_data.c_str(), post_data.length());
  server.stop();

  auto expected_response = HttpResponse{-1, ""};
  ASSERT_EQ(response.status, expected_response.status);
  ASSERT_EQ(response.raw_body, expected_response.raw_body);
  auto timer_for_req = find_timer(&registry, "ipc.client.call", "-1");
  ASSERT_TRUE(timer_for_req != nullptr);

  auto expected_tags = Tags{
    {"http.method", "POST"},
    {"http.status", "-1"},
    {"ipc.attempt", "initial"},
    {"ipc.attempt.final", "true"},
    {"ipc.endpoint", "/foo"},
    {"ipc.result", "failure"},
    {"ipc.status", "timeout"},
    {"nf.process", "spectatord"},
  };
  EXPECT_EQ(expected_tags, timer_for_req->MeterId().GetTags());
}

TEST(HttpTest, ConnectTimeout) {
  using spectator::HttpClient;
  TestRegistry registry{GetConfiguration()};
  auto logger = registry.GetLogger();

  HttpClient client{&registry, get_cfg(100, 100)};
  const std::string url = "http://192.168.255.255:81/foo";
  const std::string post_data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  auto response = client.Post(url, "Content-type: application/json", post_data);

  auto expected_response = HttpResponse{-1, ""};
  ASSERT_EQ(response.status, expected_response.status);
  ASSERT_EQ(response.raw_body, expected_response.raw_body);

  auto meters = registry.Timers();
  for (const auto& m : meters) {
    logger->info("{}", m->MeterId().GetTags());
  }
}

TEST(HttpTest, PostHeaders) {
  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = spectatord::Logger();
  logger->info("Server started on port {}", port);

  TestRegistry registry{GetConfiguration()};
  auto cfg = get_cfg(5000, 5000);
  cfg.compress = false;
  cfg.read_body = true;
  cfg.read_headers = true;
  HttpClient client{&registry, cfg};
  auto url = fmt::format("http://localhost:{}/hdr", port);
  std::string payload{"stuff"};
  // not interested in the output from the server
  auto resp = client.Post(url, "Content-Type: text/plain", payload);
  auto expected_body = std::string("header body: ok\n");
  EXPECT_EQ(resp.raw_body, expected_body);
  EXPECT_EQ(resp.status, 200);

  auto expected_len = fmt::format("{}", expected_body.length());
  EXPECT_EQ(resp.headers.size(), 2);
  EXPECT_EQ(resp.headers["Content-Length"], expected_len);
  EXPECT_EQ(resp.headers["X-Test"], "some server");
  server.stop();
}

TEST(HttpTest, Get) {
  auto cfg = get_cfg(5000, 5000);
  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, cfg};

  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = spectatord::Logger();
  logger->info("Server started on port {}", port);

  auto response = client.Get(fmt::format("http://localhost:{}/get", port));
  server.stop();
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(response.raw_body, "InsightInstanceProfile");

  // one timer, plus one counter for the percentile bucket
  EXPECT_EQ(my_timers(registry).size(), 1);
  EXPECT_EQ(my_counters(registry).size(), 1);

  spectator::Tags timer_tags{
    {"http.method", "GET"},
    {"http.status", "200"},
    {"ipc.attempt", "initial"},
    {"ipc.attempt.final", "true"},
    {"ipc.endpoint", "/get"},
    {"ipc.result", "success"},
    {"ipc.status", "success"},
    {"nf.process", "spectatord"},
  };

  auto timer_id = Id::Of("ipc.client.call", std::move(timer_tags));
  auto timer = registry.GetTimer(timer_id);
  EXPECT_EQ(timer->Count(), 1);
}

TEST(HttpTest, Get503) {
  auto cfg = get_cfg(5000, 5000);
  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, cfg};

  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = spectatord::Logger();
  logger->info("Server started on port {}", port);

  auto response = client.Get(fmt::format("http://localhost:{}/get503", port));
  server.stop();
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(response.raw_body, "InsightInstanceProfile");

  // 2 percentile timers, one for success, one for error
  auto ipc_meters = my_timers(registry);
  EXPECT_EQ(ipc_meters.size(), 2);

  spectator::Tags err_timer_tags{
    {"http.method", "GET"},
    {"http.status", "503"},
    {"ipc.attempt", "initial"},
    {"ipc.attempt.final", "false"},
    {"ipc.endpoint", "/get503"},
    {"ipc.result", "failure"},
    {"ipc.status", "http_error"},
    {"nf.process", "spectatord"},
  };
  spectator::Tags success_timer_tags{
    {"http.method", "GET"},
    {"http.status", "200"},
    {"ipc.attempt", "second"},
    {"ipc.attempt.final", "true"},
    {"ipc.endpoint", "/get503"},
    {"ipc.result", "success"},
    {"ipc.status", "success"},
    {"nf.process", "spectatord"},
  };
  auto err_id = Id::Of("ipc.client.call", std::move(err_timer_tags));
  auto err_timer = registry.GetTimer(err_id);
  EXPECT_EQ(err_timer->Count(), 1);

  auto success_timer = registry.GetTimer("ipc.client.call", std::move(success_timer_tags));
  EXPECT_EQ(success_timer->Count(), 1);
}

void test_method_header(const std::string& method) {
  auto cfg = get_cfg(5000, 5000);
  TestRegistry registry{GetConfiguration()};
  HttpClient client{&registry, cfg};

  http_server server;
  server.start();

  auto port = server.get_port();
  ASSERT_TRUE(port > 0) << "Port = " << port;
  auto logger = spectatord::Logger();
  logger->info("Server started on port {}", port);
  std::vector<std::string> headers{"X-Spectator: foo", "X-Other-Header: bar"};

  auto url = fmt::format("http://localhost:{}/getheader", port);
  auto response = method == "GET" ? client.Get(url, headers) : client.Put(url, headers);

  server.stop();
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(response.raw_body, "x-other-header: bar\nx-spectator: foo\n");
  EXPECT_EQ(my_timers(registry).size(), 1);

  spectator::Tags timer_tags{
    {"http.method", method},
    {"http.status", "200"},
    {"ipc.attempt", "initial"},
    {"ipc.attempt.final", "true"},
    {"ipc.endpoint", "/getheader"},
    {"ipc.result", "success"},
    {"ipc.status", "success"},
    {"nf.process", "spectatord"},
  };
  auto timer_id = Id::Of("ipc.client.call", std::move(timer_tags));
  auto timer = registry.GetTimer(timer_id);
  EXPECT_EQ(timer->Count(), 1);
}

TEST(HttpTest, GetHeader) { test_method_header("GET"); }

TEST(HttpTest, PutHeader) { test_method_header("PUT"); }
}  // namespace
