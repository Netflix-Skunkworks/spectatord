#include "../spectator/config.h"
#include "../spectator/registry.h"
#include "admin_server.h"
#include "gtest/gtest.h"
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>

namespace {

using namespace Poco::Dynamic;
using namespace Poco::JSON;
using namespace Poco::Net;

using spectator::Id;
using spectator::Tags;

constexpr const char* const kDefaultHost = "localhost";
constexpr int kDefaultPort = 8080;

std::unique_ptr<spectator::Config> GetConfiguration() {
  auto config = std::make_unique<spectator::Config>();
  config->age_gauge_limit = 10;
  config->batch_size = 10000; // Internal NFLX default
  config->frequency = absl::Seconds(5);  // Internal NFLX default
  return config;
}

bool contains(const std::vector<std::string>& vec, const std::string& key) {
  if (std::find(vec.begin(), vec.end(), key) != vec.end()) {
    return true;
  }
  return false;
}

/** Add operator overload function to solve for the following compilation error:
 *
 * include/fmt/core.h:2001:35: error: ambiguous overload for ‘operator==’ (operand types are ‘const char* const’ and ‘fmt::v8::basic_string_view<char>’)
 * include/Poco/Dynamic/Var.h:2008:13: note: candidate: ‘bool Poco::Dynamic::operator==(const bool&, const Poco::Dynamic::Var&)’
 * include/Poco/Dynamic/Var.h:2024:13: note: candidate: ‘bool Poco::Dynamic::operator==(const string&, const Poco::Dynamic::Var&)’
 * include/Poco/Dynamic/Var.h:2056:13: note: candidate: ‘bool Poco::Dynamic::operator==(const char*, const Poco::Dynamic::Var&)’
 */
inline bool operator == (const char* a, fmt::basic_string_view<char> b) {
  if (strlen(a) != b.size()) {
    return false;
  }

  std::string::size_type i = 0;
  for (auto p{ a }; *p; ++p) {
    if (*p != b[i]) {
      return false;
    }
    i++;
  }

  return true;
}

class AdminServerTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (server == nullptr) {
      auto logger = spectatord::Logger();
      logger->info("Start test instance of AdminServer on {}/tcp", kDefaultPort);
      registry = new spectator::Registry(GetConfiguration(), logger);
      registry->GetAgeGauge("ag1");
      registry->GetAgeGauge("ag2", Tags{{"some.tag", "one"}});
      registry->GetAgeGauge("ag3", Tags{{"some.tag", "one"}, {"other.tag", "two"}});
      server = new admin::AdminServer(*registry, kDefaultPort);
      server->Start();
    }
  }

  static void TearDownTestSuite() {
    auto logger = spectatord::Logger();
    logger->info("Shutdown test instance of AdminServer");

    if (server != nullptr) {
      server->Stop();
      delete server;
      server = nullptr;
    }

    if (registry != nullptr) {
      registry->Stop();
      delete registry;
      registry = nullptr;
    }
  }

  static spectator::Registry *registry;
  static admin::AdminServer *server;
};

spectator::Registry* AdminServerTest::registry = nullptr;
admin::AdminServer* AdminServerTest::server = nullptr;

TEST_F(AdminServerTest, EqualsOperator) {
  EXPECT_TRUE((const char*) "foobar" == (fmt::basic_string_view<char>) "foobar");
  EXPECT_FALSE((const char*) "foobar" == (fmt::basic_string_view<char>) "foo");
  EXPECT_FALSE((const char*) "foobar" == (fmt::basic_string_view<char>) "foobaz");
}

TEST_F(AdminServerTest, Join) {
  std::vector<std::string> strings {"a", "b", "c"};
  EXPECT_EQ(admin::join(strings, ","), "a,b,c");
}

TEST_F(AdminServerTest, Parse_Id) {
  spectator::Id id = Id{"g", {}};
  spectator::Id parsed_id = admin::parse_id("g");
  EXPECT_EQ(parsed_id, id);

  id = Id{"g", Tags{{"some.tag", "one"}}};
  parsed_id = admin::parse_id("g,some.tag=one");
  EXPECT_EQ(parsed_id, id);

  id = Id{"g", Tags{{"some.tag", "one"}, {"other.tag", "two"}}};
  parsed_id = admin::parse_id("g,some.tag=one,other.tag=two");
  EXPECT_EQ(parsed_id, id);

  id = Id{"g", Tags{{"some.tag", "one"}, {"other.tag", "two_10"}}};
  parsed_id = admin::parse_id("g,some.tag=one,other.tag=two:10");
  EXPECT_EQ(parsed_id, id);
}

TEST_F(AdminServerTest, GET_root) {
  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_GET, "/");
  s.sendRequest(req);

  HTTPResponse res;
  std::istream& rr = s.receiveResponse(res);

  EXPECT_EQ(res.getStatus(), 200);
  EXPECT_EQ(res.getContentType(), "application/json");

  Parser parser;
  Var result {parser.parse(rr)};
  Object::Ptr object = result.extract<Object::Ptr>();

  int count = 0;
  std::vector<std::string> expected_keys {"description", "endpoints"};
  for (auto &it : *object) {
    if (contains(expected_keys, it.first)) {
      count += 1;
    }
  }

  EXPECT_EQ(count, expected_keys.size());
}

TEST_F(AdminServerTest, GET_config) {
  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_GET, "/config");
  s.sendRequest(req);

  HTTPResponse res;
  std::istream& rr = s.receiveResponse(res);

  EXPECT_EQ(res.getStatus(), 200);
  EXPECT_EQ(res.getContentType(), "application/json");

  Parser parser;
  Var result {parser.parse(rr)};
  Object::Ptr object = result.extract<Object::Ptr>();

  std::vector<std::string> expected_keys {"age_gauge_limit", "batch_size", "common_tags",
                                         "connect_timeout", "expiration_frequency",
                                         "external_enabled", "frequency", "metatron_dir",
                                         "meter_ttl", "read_timeout", "status_metrics_enabled",
                                         "uri"};
  std::vector<std::string> found_keys;

  for (auto &it : *object) {
    found_keys.emplace_back(it.first);
  }
  std::sort(found_keys.begin(), found_keys.end());

  EXPECT_EQ(found_keys, expected_keys);
}

TEST_F(AdminServerTest, GET_config_common_tags) {
  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_GET, "/config/common_tags");
  s.sendRequest(req);

  HTTPResponse res;
  std::istream& rr = s.receiveResponse(res);
  std::string content(std::istreambuf_iterator<char>(rr), { });

  EXPECT_EQ(res.getStatus(), 200);
  EXPECT_EQ(res.getContentType(), "application/json");
  EXPECT_EQ(content.size(), 420);
}

TEST_F(AdminServerTest, GET_metrics) {
  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_GET, "/metrics");
  s.sendRequest(req);

  HTTPResponse res;
  std::istream& rr = s.receiveResponse(res);

  EXPECT_EQ(res.getStatus(), 200);
  EXPECT_EQ(res.getContentType(), "application/json");

  Parser parser;
  Var result {parser.parse(rr)};
  Object::Ptr object = result.extract<Object::Ptr>();

  std::vector<std::string> expected_keys {"age_gauges", "counters", "dist_summaries", "gauges",
                                         "max_gauges", "mono_counters", "mono_counters_uint",
                                         "stats", "timers"};
  std::vector<std::string> found_keys;

  for (auto &it : *object) {
    found_keys.emplace_back(it.first);
  }
  std::sort(found_keys.begin(), found_keys.end());

  EXPECT_EQ(found_keys, expected_keys);
}

TEST_F(AdminServerTest, GET_undefined_route) {
  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_GET, "/foo");
  s.sendRequest(req);

  HTTPResponse res;
  s.receiveResponse(res);

  EXPECT_EQ(res.getStatus(), 404);
  EXPECT_EQ(res.getReason(), "Not Found");
  EXPECT_EQ(res.getContentType(), "");
}

HTTPResponse post(const std::string& uri, const std::string& body) {
  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_POST, uri);
  s.sendRequest(req) << body;
  HTTPResponse res;
  s.receiveResponse(res);
  return res;
}

void expect_bad_request(const HTTPResponse& res) {
  EXPECT_EQ(res.getStatus(), 400);
  EXPECT_EQ(res.getReason(), "Bad Request");
  EXPECT_EQ(res.getContentType(), "application/json");
}

TEST_F(AdminServerTest, common_tags_Invalid_JSON) {
  HTTPResponse res = post("/config/common_tags", "{");
  expect_bad_request(res);

  res = post("/config/common_tags", "[");
  expect_bad_request(res);

  res = post("/config/common_tags", R"(["nf.app": "foo", "nf.wat": "bar"])");
  expect_bad_request(res);
}

TEST_F(AdminServerTest, common_tags_Array_Not_Allowed) {
  // an object is expected
  HTTPResponse res = post("/config/common_tags", R"(["foo", "bar", "baz"])");
  expect_bad_request(res);
}

TEST_F(AdminServerTest, common_tags_Disallowed_Keys) {
  // all keys invalid
  HTTPResponse res = post("/config/common_tags", R"({"nf.app": "foo", "nf.stack": "bar"})");
  expect_bad_request(res);

  // some keys invalid
  res = post("/config/common_tags", R"({"nf.app": "foo", "mantisJobId": "bar"})");
  expect_bad_request(res);

  res = post("/config/common_tags", R"({"mantisJobId": "foo", "nf.app": "yolo", "mantisJobName": "bar", "mantisUser": ""})");
  expect_bad_request(res);
}

TEST_F(AdminServerTest, common_tags_Allowed_Keys_Invalid_Values) {
  HTTPResponse res = post("/config/common_tags", R"({"mantisJobId": "foo", "mantisJobName": "bar", "mantisUser": 1})");
  expect_bad_request(res);
}

Object::Ptr get_common_tags() {
  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_GET, "/config");
  s.sendRequest(req);
  HTTPResponse res;
  std::istream& rr = s.receiveResponse(res);

  Parser parser;
  Var result {parser.parse(rr)};
  const auto& object = result.extract<Object::Ptr>();
  Var common_tags = object->get("common_tags");
  return common_tags.extract<Object::Ptr>();
}

TEST_F(AdminServerTest, set_common_tags) {
  // start with zero common tags
  Object::Ptr subObject = get_common_tags();
  EXPECT_EQ(subObject->size(), 0);

  // set three common tags
  post("/config/common_tags", R"({"mantisJobId": "foo", "mantisJobName": "bar", "mantisUser": "baz"})");
  subObject = get_common_tags();

  EXPECT_EQ(subObject->size(), 3);
  EXPECT_EQ("foo", subObject->get("mantisJobId"));
  EXPECT_EQ("bar", subObject->get("mantisJobName"));
  EXPECT_EQ("baz", subObject->get("mantisUser"));

  // change one tag, delete one tag
  post("/config/common_tags", R"({"mantisJobName": "quux", "mantisUser": ""})");
  subObject = get_common_tags();

  EXPECT_EQ(subObject->size(), 2);
  EXPECT_EQ("foo", subObject->get("mantisJobId"));
  EXPECT_EQ("quux", subObject->get("mantisJobName"));

  // reset all tags
  post("/config/common_tags", R"({"mantisJobId": "", "mantisJobName": ""})");
  subObject = get_common_tags();

  EXPECT_EQ(subObject->size(), 0);
}

Poco::JSON::Array::Ptr get_metrics(const std::string& type) {
  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_GET, "/metrics");
  s.sendRequest(req);
  HTTPResponse res;
  std::istream& rr = s.receiveResponse(res);

  Parser parser;
  Var result {parser.parse(rr)};
  const auto& object = result.extract<Object::Ptr>();
  Var metrics;
  if (type == "A") {
    metrics = object->get("age_gauges");
  } else if (type == "g") {
    metrics = object->get("gauges");
  }

  return metrics.extract<Poco::JSON::Array::Ptr>();
}

HTTPResponse delete_age_gauge(const std::string& id) {
  std::string uri;
  if (id.empty()) {
    uri = "/metrics/A";
  } else {
    uri = "/metrics/A/" + id;
  }

  HTTPClientSession s(kDefaultHost, kDefaultPort);
  HTTPRequest req(HTTPRequest::HTTP_DELETE, uri);
  s.sendRequest(req);

  HTTPResponse res;
  s.receiveResponse(res);

  return res;
}

TEST_F(AdminServerTest, delete_meters) {
  // start with three pre-populated age gauges
  Poco::JSON::Array::Ptr subArr = get_metrics("A");
  EXPECT_EQ(subArr->size(), 3);

  HTTPResponse res;

  res = delete_age_gauge("ag1");
  EXPECT_EQ(200, res.getStatus());

  res = delete_age_gauge("ag1");
  EXPECT_EQ(404, res.getStatus());

  res = delete_age_gauge("ag3,some.tag=one,other.tag=two");
  EXPECT_EQ(200, res.getStatus());

  // bad id formats will get mangled into the last tag, and thus not match
  res = delete_age_gauge("ag1,foo=bar:10");
  EXPECT_EQ(404, res.getStatus());

  // delete all remaining age gauges
  res = delete_age_gauge("");
  EXPECT_EQ(200, res.getStatus());

  subArr = get_metrics("A");
  EXPECT_EQ(subArr->size(), 0);
}

}
