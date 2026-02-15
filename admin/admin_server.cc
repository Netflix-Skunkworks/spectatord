#include "admin_server.h"
#include "../server/spectatord.h"
#include "../spectator/version.h"
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <algorithm>
#include <numeric>

namespace admin
{

using namespace Poco::Dynamic;
using namespace Poco::JSON;
using namespace Poco::Net;

using admin::AdminServer;
using admin::RequestHandler;
using admin::RequestHandlerFactory;

using spectator::Id;
using spectator::Tags;
using spectatord::get_measurement;

std::string join(std::vector<std::string> const& strings, const std::string& delimiter)
{
	return std::accumulate(std::next(strings.begin()), strings.end(), strings[0],
	                       [&delimiter](const std::string& a, const std::string& b) { return a + delimiter + b; });
}

void GET_root(HTTPServerRequest& req, HTTPServerResponse& res)
{
	res.setStatus(HTTPResponse::HTTP_OK);
	res.setContentType("application/json");
	res.send() << "{"
	           << R"("description": "SpectatorD Admin Server )" << VERSION << R"(",)"
	           << R"("endpoints": [)"
	           << R"("http://)" << req.getHost() << R"(",)"
	           << R"("http://)" << req.getHost() << R"(/config",)"
	           << R"("http://)" << req.getHost() << R"(/config/common_tags",)"
	           << R"("http://)" << req.getHost() << R"(/metrics")"
	           << "]"
	           << "}";
}

void GET_config(HTTPServerRequest& req, HTTPServerResponse& res, const spectator::Config& config)
{
	Object::Ptr obj = new Object(true);

	Object::Ptr common_tags = new Object(true);
	for (auto& pair : config.common_tags)
	{
		common_tags->set(pair.first, pair.second);
	}

	obj->set("age_gauge_limit", config.age_gauge_limit);
	obj->set("batch_size", config.batch_size);
	obj->set("common_tags", common_tags);
	obj->set("connect_timeout", ToDoubleMilliseconds(config.connect_timeout));
	obj->set("expiration_frequency", ToDoubleMilliseconds(config.expiration_frequency));
	obj->set("external_enabled", config.external_enabled);
	obj->set("frequency", ToDoubleMilliseconds(config.frequency));
	obj->set("metatron_dir", config.metatron_dir);
	obj->set("meter_ttl", ToDoubleMilliseconds(config.meter_ttl));
	obj->set("read_timeout", ToDoubleMilliseconds(config.read_timeout));
	obj->set("status_metrics_enabled", config.status_metrics_enabled);
	obj->set("uri", config.uri);

	res.setStatus(HTTPResponse::HTTP_OK);
	res.setContentType("application/json");
	obj->stringify(res.send());
}

void GET_config_common_tags(HTTPServerRequest& req, HTTPServerResponse& res, const std::string& allowed_tags)
{
	std::string usage =
	    "To configure SpectatorD common tags, POST a JSON object to this endpoint with "
	    "key-value pairs defining the desired common tags. To delete a tag, set the value "
	    "to an empty string. Attempting to configure any other tags besides the allowed "
	    "set will return an error. Only the following tags may be modified: ";
	usage += allowed_tags + ".";

	res.setStatus(HTTPResponse::HTTP_OK);
	res.setContentType("application/json");
	res.send() << "{" << R"("usage":")" << usage << R"(")" << "}";
}

void POST_config_common_tags(HTTPServerRequest& req, HTTPServerResponse& res, std::shared_ptr<spdlog::logger>& logger,
                             const std::vector<std::string>& allowed_tags, spectator::Registry& registry)
{
	Parser parser;
	Var result;

	try
	{
		result = parser.parse(req.stream());
	}
	catch (Poco::Exception& e)
	{
		logger->error("POST /config/common_tags json parse error: {}", e.message());
		res.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
		res.setReason("Bad Request");
		res.setContentType("application/json");
		res.send() << R"({"message": "json parse exception"})";
		return;
	}

	if (result.type() != typeid(Object::Ptr))
	{
		res.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
		res.setReason("Bad Request");
		res.setContentType("application/json");
		res.send() << R"({"message": "expected a json object"})";
		return;
	}

	Object::Ptr object = result.extract<Object::Ptr>();

	for (auto& it : *object)
	{
		if (std::find(allowed_tags.begin(), allowed_tags.end(), it.first) == allowed_tags.end())
		{
			res.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
			res.setReason("Bad Request");
			res.setContentType("application/json");
			res.send() << R"({"message": "only allowed tags may be set"})";
			return;
		}
		if (it.second.type() != typeid(std::string))
		{
			res.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
			res.setReason("Bad Request");
			res.setContentType("application/json");
			res.send() << R"({"message": "tag values must be strings"})";
			return;
		}
	}

	for (auto& it : *object)
	{
		auto second = it.second.extract<std::string>();
		if (second.empty())
		{
			logger->info("delete common tag {}", it.first);
			registry.EraseCommonTag(it.first);
		}
		else
		{
			logger->info("update common tag {}={}", it.first, second);
			registry.UpdateCommonTag(it.first, second);
		}
	}

	res.setStatus(HTTPResponse::HTTP_OK);
	res.setContentType("application/json");
	res.send() << R"({"message": "common tags updated"})";
}

Poco::SharedPtr<Object> fmt_meter_object(spectator::Meter* meter)
{
	Object::Ptr obj = new Object(true);
	obj->set("name", fmt::format("{}", meter->MeterId().Name()));

	Object::Ptr tag_obj = new Object(true);
	for (auto tag_it : meter->MeterId().GetTags())
	{
		tag_obj->set(fmt::format("{}", tag_it.key), fmt::format("{}", tag_it.value));
	}
	obj->set("tags", tag_obj);

	return obj;
}

void GET_metrics(HTTPServerRequest& req, HTTPServerResponse& res, spectator::Registry& registry)
{
	Object::Ptr obj = new Object(true);

	Poco::JSON::Array::Ptr age_gauges = new Poco::JSON::Array(true);
	for (auto it : registry.AgeGauges())
	{
		auto meter = fmt_meter_object((spectator::Meter*)it);
		meter->set("value", fmt::format("{}", it->Value()));
		age_gauges->add(meter);
	}
	obj->set("age_gauges", age_gauges);

	Poco::JSON::Array::Ptr counters = new Poco::JSON::Array(true);
	for (auto it : registry.Counters())
	{
		auto meter = fmt_meter_object((spectator::Meter*)it);
		meter->set("value", fmt::format("{}", it->Count()));
		counters->add(meter);
	}
	obj->set("counters", counters);

	Poco::JSON::Array::Ptr dist_summaries = new Poco::JSON::Array(true);
	for (auto it : registry.DistSummaries())
	{
		auto meter = fmt_meter_object((spectator::Meter*)it);
		meter->set("value", fmt::format("{}", it->TotalAmount()));
		dist_summaries->add(meter);
	}
	obj->set("dist_summaries", dist_summaries);

	Poco::JSON::Array::Ptr gauges = new Poco::JSON::Array(true);
	for (auto it : registry.Gauges())
	{
		auto meter = fmt_meter_object((spectator::Meter*)it);
		meter->set("value", fmt::format("{}", it->Get()));
		gauges->add(meter);
	}
	obj->set("gauges", gauges);

	Poco::JSON::Array::Ptr max_gauges = new Poco::JSON::Array(true);
	for (auto it : registry.MaxGauges())
	{
		auto meter = fmt_meter_object((spectator::Meter*)it);
		meter->set("value", fmt::format("{}", it->Get()));
		max_gauges->add(meter);
	}
	obj->set("max_gauges", max_gauges);

	Poco::JSON::Array::Ptr mono_counters = new Poco::JSON::Array(true);
	for (auto it : registry.MonotonicCounters())
	{
		auto meter = fmt_meter_object((spectator::Meter*)it);
		meter->set("value", fmt::format("{}", it->Delta()));
		mono_counters->add(meter);
	}
	obj->set("mono_counters", mono_counters);

	Poco::JSON::Array::Ptr mono_counters_uint = new Poco::JSON::Array(true);
	for (auto it : registry.MonotonicCountersUint())
	{
		auto meter = fmt_meter_object((spectator::Meter*)it);
		meter->set("value", fmt::format("{}", it->Delta()));
		mono_counters_uint->add(meter);
	}
	obj->set("mono_counters_uint", mono_counters_uint);

	Poco::JSON::Array::Ptr timers = new Poco::JSON::Array(true);
	for (auto it : registry.Timers())
	{
		auto meter = fmt_meter_object((spectator::Meter*)it);
		meter->set("value", fmt::format("{}", it->TotalTime()));
		timers->add(meter);
	}
	obj->set("timers", timers);

	Object::Ptr stats = new Object(true);
	auto age_gauges_size = registry.AgeGauges().size();
	auto counters_size = registry.Counters().size();
	auto dist_summaries_size = registry.DistSummaries().size();
	auto gauges_size = registry.Gauges().size();
	auto max_gauges_size = registry.MaxGauges().size();
	auto mono_counters_size = registry.MonotonicCounters().size();
	auto mono_counters_uint_size = registry.MonotonicCountersUint().size();
	auto timers_size = registry.Timers().size();
	auto total = age_gauges_size + counters_size + dist_summaries_size + gauges_size + max_gauges_size +
	             mono_counters_size + mono_counters_uint_size + timers_size;
	stats->set("age_gauges.size", age_gauges_size);
	stats->set("counters.size", counters_size);
	stats->set("dist_summaries.size", dist_summaries_size);
	stats->set("gauges.size", gauges_size);
	stats->set("max_gauges.size", max_gauges_size);
	stats->set("mono_counters.size", mono_counters_size);
	stats->set("mono_counters_uint.size", mono_counters_uint_size);
	stats->set("timers.size", timers_size);
	stats->set("total.size", total);
	obj->set("stats", stats);

	res.setStatus(HTTPResponse::HTTP_OK);
	res.setContentType("application/json");
	obj->stringify(res.send());
}

spectator::Id parse_id(const std::string& id_str)
{
	// get name (tags are specified with , but are optional)
	auto pos = id_str.find_first_of(',');
	if (pos == std::string_view::npos)
	{
		return Id{id_str, Tags{}};
	}

	auto name = id_str.substr(0, pos);
	spectator::Tags tags{};

	// optionally get tags
	if (id_str[pos] == ',')
	{
		while (pos != std::string_view::npos)
		{
			++pos;
			auto k_pos = id_str.find('=', pos);
			if (k_pos == std::string_view::npos) break;
			auto key = id_str.substr(pos, k_pos - pos);
			++k_pos;
			auto v_pos = id_str.find_first_of(',', k_pos);
			std::string val;
			if (v_pos == std::string_view::npos)
			{
				tags.add(key, id_str.substr(k_pos, id_str.length() - k_pos));
				break;
			}
			else
			{
				tags.add(key, id_str.substr(k_pos, v_pos - k_pos));
			}
			pos = v_pos;
		}
	}

	return spectator::Id{name, tags};
}

void DELETE_metrics(const std::string& type, HTTPServerRequest& req, HTTPServerResponse& res,
                    std::shared_ptr<spdlog::logger>& logger, spectator::Registry& registry)
{
	if (req.getURI().size() == 10)
	{
		logger->info("DELETE /metrics/{} succeeded: all meters deleted", type);
		registry.DeleteAllMeters(type);
		res.setStatus(HTTPResponse::HTTP_OK);
		res.send();
	}
	else
	{
		auto id = parse_id(req.getURI().substr(11));
		if (registry.DeleteMeter(type, id))
		{
			logger->info("DELETE /metrics/{} succeeded: '{}'", type, id);
			res.setStatus(HTTPResponse::HTTP_OK);
			res.send();
		}
		else
		{
			logger->error("DELETE /metrics/{} failed: meter not found '{}'", type, id);
			res.setStatus(HTTPResponse::HTTP_NOT_FOUND);
			res.setReason("Not Found");
			res.send();
		}
	}
}

void RequestHandler::handleRequest(HTTPServerRequest& req, HTTPServerResponse& res)
{
	this->logger->debug("AdminServer request for URI={}", req.getURI());

	auto localhost_found = req.getHost().rfind("localhost") != std::string::npos ||
	                       req.getHost().rfind("127.0.0.1") != std::string::npos ||
	                       req.getHost().rfind("[::1]") != std::string::npos;

	if (req.getURI() == "/" && req.getMethod() == "GET")
	{
		// describe admin service
		GET_root(req, res);
	}
	else if (req.getURI() == "/config" && req.getMethod() == "GET")
	{
		// get the full spectator configuration
		GET_config(req, res, this->registry.GetConfig());
	}
	else if (req.getURI() == "/config/common_tags" && req.getMethod() == "GET")
	{
		// describe common_tags usage
		GET_config_common_tags(req, res, join(this->allowed_tags, ", "));
	}
	else if (req.getURI() == "/config/common_tags" && req.getMethod() == "POST")
	{
		// update the common_tags config
		if (localhost_found)
		{
			POST_config_common_tags(req, res, this->logger, this->allowed_tags, this->registry);
		}
		else
		{
			this->logger->error("AdminServer endpoint may only be accessed from localhost method={} uri={} ",
			                    req.getMethod(), req.getURI());
			res.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
			res.setReason("Bad Request");
			res.send();
		}
	}
	else if (req.getURI() == "/metrics" && req.getMethod() == "GET")
	{
		// get all metrics defined in the registry
		GET_metrics(req, res, this->registry);
	}
	else if (req.getURI().rfind("/metrics/A", 0) == 0 && req.getMethod() == "DELETE")
	{
		// delete one or all age gauges
		if (localhost_found)
		{
			DELETE_metrics("A", req, res, this->logger, this->registry);
		}
		else
		{
			this->logger->error("AdminServer endpoint may only be accessed from localhost method={} uri={} ",
			                    req.getMethod(), req.getURI());
			res.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
			res.setReason("Bad Request");
			res.send();
		}
	}
	else if (req.getURI().rfind("/metrics/g", 0) == 0 && req.getMethod() == "DELETE")
	{
		// delete one or all gauges
		if (localhost_found)
		{
			DELETE_metrics("g", req, res, this->logger, this->registry);
		}
		else
		{
			this->logger->error("AdminServer endpoint may only be accessed from localhost method={} uri={} ",
			                    req.getMethod(), req.getURI());
			res.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
			res.setReason("Bad Request");
			res.send();
		}
	}
	else
	{
		this->logger->error("AdminServer unknown endpoint method={} uri={} ", req.getMethod(), req.getURI());
		res.setStatus(HTTPResponse::HTTP_NOT_FOUND);
		res.setReason("Not Found");
		res.send();
	}
}

}  // namespace admin