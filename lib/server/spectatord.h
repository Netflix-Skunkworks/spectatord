#pragma once

#include "expiring_cache.h"
#include "id_cache.h"
#include "server/transport/handler.h"
#include "spectator/registry/distinct_count_sketch.h"
#include "spectator/registry/percentile_distribution_summary.h"
#include "spectator/registry/percentile_timer.h"
#include "spectator/registry/registry.h"

namespace spectatord
{

class Server
{
   public:
	Server(bool ipv4_only, int port_number, std::optional<int> statsd_port_number,
	       std::optional<std::string> socket_path, spectator::Registry* registry);
	Server(const Server&) = delete;
	Server(Server&&) = delete;
	Server& operator=(const Server&) = delete;
	Server& operator=(Server&&) = delete;

	~Server() = default;

	void Start();
	void Stop();

   private:
	bool ipv4_only_;
	int port_number_;
	std::optional<int> statsd_port_number_;
	std::optional<std::string> socket_path_;
	spectator::Registry* registry_;
	std::shared_ptr<spectator::Counter> parsed_count_;
	std::shared_ptr<spectator::Counter> parse_errors_;
	std::shared_ptr<spdlog::logger> logger_;
	expiring_cache<spectator::PercentileTimer> perc_timers_;
	expiring_cache<spectator::PercentileDistributionSummary> perc_ds_;
	expiring_cache<spectator::DistinctCountSketch> sketches_;
	// caches the parsed Id for each (line type, id text) seen by parse_line
	id_cache id_cache_;

	std::atomic_bool should_stop_{false};
	std::thread upkeep_thread_;  // some janitorial tasks, like expiration of
	                             // cache entries and generation of metrics
	std::mutex cv_mutex_;
	std::condition_variable cv_;
	void upkeep();
	void update_network_metrics();

	std::optional<std::string> parse_lines(char* buffer, const handler_t& parser);
	std::optional<std::string> parse_line(const char* buffer);
	std::optional<std::string> parse_statsd_line(const char* buffer);
	void ensure_not_stuck();

   protected:
	// for tests
	auto id_cache_stats() const -> id_cache::stats { return id_cache_.get_stats(); }
	auto id_cache_clear_if_saturated() -> bool { return id_cache_.clear_if_saturated(); }
	void id_cache_clear_for_test() { id_cache_.clear_for_test(); }

	std::optional<std::string> parse(char* buffer);
	std::optional<std::string> parse_statsd(char* buffer);
};

union valueT
{
	double d;
	uint64_t u;
};

struct measurement
{
	spectator::Id id;
	valueT value;
	// Raw (un-parsed) value token, used by the distinct count sketch 'S' type whose value is a
	// base64 blob to hash rather than a number. Points into the parse buffer.
	std::string_view str_value{};
};

// Parses one measurement. When id_len is given it receives the length of the
// identity portion, which is not always the offset of the first ':': a tag key
// is located with find('='), so it may span a colon.
std::optional<measurement> get_measurement(char type, std::string_view measurement_str, std::string* err_msg,
                                          size_t* id_len = nullptr);

}  // namespace spectatord
