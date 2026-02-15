#pragma once

#include "../spectator/registry.h"
#include "../util/logger.h"
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>

namespace admin
{

using namespace Poco::Net;
using namespace spectator;

std::string join(std::vector<std::string> const& strings, const std::string& delimiter);
spectator::Id parse_id(const std::string& id_str);

class RequestHandler : public Poco::Net::HTTPRequestHandler
{
	spectator::Registry& registry;
	std::shared_ptr<spdlog::logger> logger = spectatord::Logger();

	// tags which may be modified by the config/common_tags endpoint
	std::vector<std::string> allowed_tags{"mantisJobId",       "mantisJobName",      "mantisUser",
	                                      "mantisWorkerIndex", "mantisWorkerNumber", "mantisWorkerStageNumber"};

   public:
	explicit RequestHandler(Registry& r) : registry{r} {}
	void handleRequest(HTTPServerRequest&, HTTPServerResponse&) override;
};

class RequestHandlerFactory : public HTTPRequestHandlerFactory
{
	Registry& registry;

   public:
	explicit RequestHandlerFactory(Registry& r) : registry{r} {}
	HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) override
	{
		return new admin::RequestHandler(this->registry);
	}
};

class AdminServer
{
	std::shared_ptr<HTTPServer> instance;
	//  Registry &registry;

   public:
	AdminServer(Registry& r, int p)
	    : instance{std::make_shared<HTTPServer>(new RequestHandlerFactory(r), SocketAddress("localhost", p),
	                                            new HTTPServerParams)}
	{
	}
	//    registry{r} {}
	void Start() { instance->start(); }
	void Stop() { instance->stop(); }
};

}  // namespace admin