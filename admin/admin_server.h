#pragma once

#include "spectator/registry.h"
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>

namespace admin {

std::string join(std::vector<std::string> const& strings, const std::string& delimiter);
spectator::Id parse_id(const std::string& id_str);

class RequestHandler: public Poco::Net::HTTPRequestHandler {
  spectator::Registry &registry;
  std::shared_ptr<spdlog::logger> logger = spectatord::Logger();

  // tags which may be modified by the config/common_tags endpoint
  std::vector<std::string> allowed_tags {
      "mantisJobId",
      "mantisJobName",
      "mantisUser",
      "mantisWorkerIndex",
      "mantisWorkerNumber",
      "mantisWorkerStageNumber"
  };

 public:
  explicit RequestHandler(spectator::Registry &r): registry{r} {}
  void handleRequest(Poco::Net::HTTPServerRequest &, Poco::Net::HTTPServerResponse &) override;
};

class RequestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory {
  spectator::Registry &registry;

 public:
  explicit RequestHandlerFactory(spectator::Registry &r): registry{r} {}
  Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest &) override {
    return new admin::RequestHandler(this->registry);
  }
};

class AdminServer {
  std::shared_ptr<Poco::Net::HTTPServer> instance;
  spectator::Registry &registry;

 public:
  AdminServer(spectator::Registry &r, int p):
    registry{r},
    instance{std::make_shared<Poco::Net::HTTPServer>(new RequestHandlerFactory(r), p)} {}
  void Start() { instance->start(); }
  void Stop() { instance->stop(); }
};

}