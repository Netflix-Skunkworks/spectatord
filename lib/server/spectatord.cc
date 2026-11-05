#include "spectatord.h"
#include "server/transport/local_server.h"
#include "server/proc/proc_utils.h"
#include "server/transport/udp_server.h"
#include "util/systemd.h"

#include <absl/strings/escaping.h>
#include <asio.hpp>

namespace spectatord
{

enum class StatsdMetricType
{
	Counter,
	Timing,
	Gauge,
	Histogram,
	Set
};

inline auto add_tag(spectator::Tags* tags, const char* begin_key, const char* end_key, const char* begin_value,
                    const char* end_value) -> bool
{
	std::string_view key;
	std::string_view value;

	if (begin_value != nullptr)
	{
		key = std::string_view{begin_key, static_cast<size_t>(end_key - begin_key)};
		value = std::string_view{begin_value, static_cast<size_t>(end_value - begin_value)};
	}
	else
	{
		key = std::string_view{begin_key, static_cast<size_t>(end_value - begin_key)};
		value = "1";  // datadog statsd allows for marker tags, like #shell
		              // so we map them to shell=1
	}
	if (key.empty() || value.empty())
	{
		return false;
	}
	tags->add(key, value);
	return true;
}

// feed lines into parser
auto Server::parse_lines(char* buffer, const handler_t& parser) -> std::optional<std::string>
{
	std::string err_msg;
	size_t num_parsed = 0;
	size_t num_errors = 0;

	char* line = buffer;
	while (line != nullptr)
	{
		// skip empty lines, the parser expects at least one character
		line += std::strspn(line, "\n");
		if (*line == '\0')
		{
			break;
		}

		// terminate the current line, and find the start of the next one
		char* next = std::strchr(line, '\n');
		if (next != nullptr)
		{
			*next++ = '\0';
		}

		if (auto maybe_err = parser(line))
		{
			++num_errors;
			if (!err_msg.empty())
			{
				err_msg += '\n';
			}
			err_msg += *maybe_err;
		}
		else
		{
			++num_parsed;
		}
		line = next;
	}

	if (registry_->GetConfig().status_metrics_enabled)
	{
		if (num_parsed > 0)
		{
			parsed_count_->Add(static_cast<double>(num_parsed));
		}
		if (num_errors > 0)
		{
			parse_errors_->Add(static_cast<double>(num_errors));
		}
	}
	if (err_msg.empty())
	{
		return {};
	}

	return err_msg;
}

static void update_statsd_metric(spectator::Registry* registry, StatsdMetricType type, spectator::Id id, double value,
                                 double sampling_rate)
{
	switch (type)
	{
		case StatsdMetricType::Counter:
			registry->GetCounter(std::move(id))->Add(value / sampling_rate);
			break;
		case StatsdMetricType::Gauge:
			// ignore sampling rate for gauges
			registry->GetGauge(std::move(id))->Set(value);
			break;
		case StatsdMetricType::Histogram:
		{
			auto k = std::lround(1 / sampling_rate);
			auto hist = registry->GetDistributionSummary(std::move(id));
			for (auto i = 0; i < k; ++i)
			{
				hist->Record(value);
			}
			break;
		}
		case StatsdMetricType::Timing:
		{
			auto k = std::lround(1 / sampling_rate);
			auto ns = std::chrono::nanoseconds(std::lround(value * 1e6));
			auto timer = registry->GetTimer(std::move(id));
			for (auto i = 0; i < k; ++i)
			{
				timer->Record(ns);
			}
			break;
		}
		case StatsdMetricType::Set:
			Logger()->info("Ignoring set cardinality metric for {}", id);
			break;
	}
}

/* StatsD Protocol Parser Specification
 *
 * Parse a single line, of the following form:
 *
 *   <METRIC_NAME>:<VALUE>|<TYPE>|@<SAMPLE_RATE>|#<TAG_KEY_1>:<TAG_VALUE_1>,<TAG_2>
 *
 * Reference Link:
 *
 *   https://docs.datadoghq.com/developers/dogstatsd/datagram_shell/?tab=metrics
 *
 * Examples:
 *
 *   custom_metric:60|g|#shell            - Set the custom_metric gauge to 60 and tag it
 *   with 'shell' page.views:1|c          - Increment the page.views COUNT metric.
 *   fuel.level:0.5|g                     - Record the fuel tank is half-empty.
 *   song.length:240|h|@0.5               - Sample the song.length histogram half of the time.
 *   users.uniques:1234|s                 - Track unique visitors to the site.
 *   users.online:1|c|#country:china      - Increment the active users COUNT metric and tag by
 *                                          country of origin.
 *   users.online:1|c|@0.5|#country:china - Track active China users and use a sample rate.
 */
auto Server::parse_statsd_line(const char* buffer) -> std::optional<std::string>
{
	assert(buffer != nullptr);

	// get name
	const char* p = std::strchr(buffer, ':');
	if (p == nullptr || p == buffer)
	{
		return "Invalid format: name is required";
	}
	std::string_view name{buffer, static_cast<size_t>(p - buffer)};

	// get value
	char* last_char = nullptr;
	++p;
	auto value = std::strtod(p, &last_char);
	if (last_char == p)
	{
		return fmt::format("Unable to parse value starting at {}", p);
	}

	p = last_char;
	StatsdMetricType type;
	if (*p != '|')
	{
		return fmt::format("Invalid format. Expected | starting at {}", p);
	}
	++p;
	char char_type = *p;
	switch (char_type)
	{
		case 'c':
			type = StatsdMetricType::Counter;
			break;
		case 'g':
			type = StatsdMetricType::Gauge;
			break;
		case 'h':
			type = StatsdMetricType::Histogram;
			break;
		case 's':
			type = StatsdMetricType::Set;
			break;
		case 'm':
			type = StatsdMetricType::Timing;
			if (*++p != 's')
			{
				return fmt::format("Invalid metric type for {}", buffer);
			}
			break;
		default:
			return fmt::format("Invalid type for name {} ({})", name, buffer);
	}
	++p;
	auto sampling_rate = 1.0;
	spectator::Tags tags{};
	if (*p == '|')
	{
		// maybe sampling rate
		++p;
		if (*p == '@')
		{
			++p;
			sampling_rate = std::strtod(p, &last_char);
			if (last_char == p || sampling_rate <= 0 || sampling_rate > 1)
			{
				return fmt::format("Invalid sampling rate for name={}", name);
			}
			p = last_char;
			if (*p == '|')
			{
				++p;
			}
		}
		// maybe tags
		if (*p == '#')
		{
			++p;
			const char* begin_key = p;
			const char* end_key = nullptr;
			const char* begin_value = nullptr;
			while (*p != '\0')
			{
				++p;
				if (*p == ':')
				{
					end_key = p;
					begin_value = ++p;
				}
				if (*p == ',')
				{
					if (!add_tag(&tags, begin_key, end_key, begin_value, p))
					{
						return fmt::format("Invalid tags for name={}", name);
					}
					begin_value = nullptr;
					begin_key = ++p;
				}
			}
			if (!add_tag(&tags, begin_key, end_key, begin_value, p))
			{
				return fmt::format("Invalid tags for name={}", name);
			}
		}
	}

	spectator::Id id{spectator::intern_str(name), std::move(tags)};
	update_statsd_metric(registry_, type, std::move(id), value, sampling_rate);
	return {};
}

struct parsed_value
{
	valueT value{};
	std::string_view str_value{};
};

// Parses the value portion of a measurement line (everything after the ':').
// value_str must be NUL-terminated: it points into the parse buffer, which
// parse_lines NUL-terminates at the end of each line.
static auto parse_value(char type, std::string_view value_str, std::string* err_msg)
    -> std::optional<parsed_value>
{
	parsed_value result{};
	if (type == 'S')
	{
		// The distinct count sketch value is an opaque base64 blob (the bytes to hash), not a
		// number. Capture the raw token; it is decoded and hashed in parse_line. Treat it like a
		// tag value with respect to size: it is bounded by the overall line size limits.
		//
		// An empty value is allowed: it decodes to zero bytes and hashes the empty input, which
		// matches record("") / record(empty bytes) in the other clients so the sketches merge.
		// A genuinely missing value (no trailing ':') is rejected earlier as a missing name.
		auto str_value = value_str;
		while (!str_value.empty() && std::isspace(static_cast<unsigned char>(str_value.back())) != 0)
		{
			str_value.remove_suffix(1);
		}
		result.str_value = str_value;
		return result;
	}

	const auto* begin = value_str.data();
	char* last_char = nullptr;
	if (type == 'U' || type == 's')
	{
		result.value.u = std::strtoull(begin, &last_char, 10);
	}
	else
	{
		result.value.d = std::strtod(begin, &last_char);
	}

	if (last_char == begin)
	{
		*err_msg = "Unable to parse value for measurement";
		return {};
	}
	if (*last_char != '\0' && std::isspace(static_cast<unsigned char>(*last_char)) == 0)
	{
		if (type == 'U' || type == 's')
		{
			*err_msg = fmt::format("Got {} parsing value, ignoring chars starting at {}", result.value.u, last_char);
		}
		else
		{
			*err_msg = fmt::format("Got {} parsing value, ignoring chars starting at {}", result.value.d, last_char);
		}
	}
	return result;
}

auto get_measurement(char type, std::string_view measurement_str, std::string* err_msg, size_t* id_len)
    -> std::optional<measurement>
{
	// get name (tags are specified with , but are optional)
	auto pos = measurement_str.find_first_of(",:");
	if (pos == std::string_view::npos || pos == 0)
	{
		*err_msg = "Missing name";
		return {};
	}
	auto name = measurement_str.substr(0, pos);
	spectator::Tags tags{};

	// Whether the id is self-contained: the tag loop below ran to the ':' that
	// ends the id, with every tag segment carrying its '='. If it breaks out
	// instead there is no '=' left in the line at all, so the text has no
	// well-defined id extent and must not be cached.
	//
	// A tag key may itself contain a colon, because it is located with
	// find('='): "m,a:b=1:5" has the id "m,a:b=1". Such an id is still
	// self-contained; it is the length check in parse_line that stops it being
	// cached under the shorter prefix "m,a".
	auto self_contained = true;

	// optionally get tags
	if (measurement_str[pos] == ',')
	{
		while (measurement_str[pos] != ':')
		{
			++pos;
			auto k_pos = measurement_str.find('=', pos);
			if (k_pos == std::string_view::npos)
			{
				self_contained = false;
				break;
			}
			auto key = measurement_str.substr(pos, k_pos - pos);
			++k_pos;
			auto v_pos = measurement_str.find_first_of(",:", k_pos);
			if (v_pos == std::string_view::npos)
			{
				*err_msg = "Missing value";
				return {};
			}
			auto val = measurement_str.substr(k_pos, v_pos - k_pos);
			tags.add(key, val);
			pos = v_pos;
		}
	}

	if (id_len != nullptr)
	{
		// Where the id ended, which is not necessarily the offset of the first
		// ':' in the line: a tag key is located with find('='), so it may span a
		// colon. Reported only for a self-contained id, because only then does
		// the same text parse the same way regardless of what follows it.
		*id_len = self_contained ? pos : std::numeric_limits<size_t>::max();
	}

	++pos;
	if (pos > measurement_str.size())
	{
		// the tag section ran to the end of the line without a value separator,
		// e.g. "name,"; substr() would throw for such a line
		*err_msg = "Missing value";
		return {};
	}
	auto value = parse_value(type, measurement_str.substr(pos), err_msg);
	if (!value)
	{
		return {};
	}
	auto name_ref = spectator::intern_str(name);
	return measurement{spectator::Id{name_ref, tags}, value->value, value->str_value};
}

static constexpr auto min_perc_timer = absl::Nanoseconds(1);
static constexpr auto max_perc_timer = absl::Hours(24);
static constexpr auto min_ds = std::numeric_limits<int64_t>::min();
static constexpr auto max_ds = std::numeric_limits<int64_t>::max();

static auto create_perc_timer(spectator::Registry* registry, spectator::Id id)
{
	return std::make_unique<spectator::PercentileTimer>(registry, std::move(id), min_perc_timer, max_perc_timer);
}

static auto create_perc_ds(spectator::Registry* registry, spectator::Id id)
{
	return std::make_unique<spectator::PercentileDistributionSummary>(registry, std::move(id), min_ds, max_ds);
}

static auto create_sketch(spectator::Registry* registry, spectator::Id id)
{
	return std::make_unique<spectator::DistinctCountSketch>(registry, std::move(id));
}

Server::Server(bool ipv4_only, int port_number, std::optional<int> statsd_port_number,
               std::optional<std::string> socket_path, spectator::Registry* registry)
    : ipv4_only_{ipv4_only},
      port_number_{port_number},
      statsd_port_number_{statsd_port_number},
      socket_path_{std::move(socket_path)},
      registry_{registry},
      parsed_count_{registry_->GetCounter("spectatord.parsedCount")},
      parse_errors_{registry_->GetCounter("spectatord.parseErrors")},
      logger_{Logger()},
      perc_timers_{create_perc_timer},
      perc_ds_{create_perc_ds},
      sketches_{create_sketch},
      id_cache_{registry->GetConfig().meter_cache_size}
{
}

static void prepare_socket_path(const std::string& socket_path)
{
	::unlink(socket_path.c_str());

	// TODO: Migrate to std::filesystem after we verify it works on all our platforms
	auto last = socket_path.rfind('/');
	if (last != std::string::npos)
	{
		auto dir = socket_path.substr(0, last);
		// create dir with the default umask
		Logger()->debug("Creating dir: {}", dir);
		::mkdir(dir.c_str(), 0777);
	}

	// We don't want to restrict the permissions on the socket path so any user can send metrics
	umask(0);
}

void Server::Start()
{
	auto logger = Logger();
	const auto& cfg = registry_->GetConfig();

	if (cfg.status_metrics_enabled)
	{
		registry_->GetAgeGauge("spectatord.uptime")->UpdateLastSuccess();
	}

	logger->info("Starting janitorial tasks");
	upkeep_thread_ = std::thread(&Server::upkeep, this);

	asio::io_context io_context;

	// stop the server on SIGINT / SIGTERM
	asio::signal_set signals(io_context, SIGINT, SIGTERM);
	signals.async_wait(
	    [&io_context, this](std::error_code /*ec*/, int /*signo*/)
	    {
		    io_context.stop();
		    this->Stop();
	    });

	logger->info("Using receive buffer size = {}", max_buffer_size());
	auto parser = [this](char* buffer) { return this->parse(buffer); };

	// Check for systemd socket activation for the main UDP port
	std::unique_ptr<UdpServer> udp_server;
	auto systemd_udp_fd = get_systemd_udp_socket(port_number_);
	if (systemd_udp_fd)
	{
		logger->info("Using systemd socket activation for spectatord server on port {}/udp (fd={})", port_number_,
		             *systemd_udp_fd);
		bool is_ipv6 = is_socket_ipv6(*systemd_udp_fd);
		udp_server = std::make_unique<UdpServer>(io_context, *systemd_udp_fd, is_ipv6, parser);
	}
	else
	{
		logger->info("Starting spectatord server on port {}/udp (ipv4_only={})", port_number_, ipv4_only_);
		udp_server = std::make_unique<UdpServer>(io_context, ipv4_only_, port_number_, parser);
	}
	udp_server->Start();

	std::unique_ptr<UdpServer> statsd_server;
	if (statsd_port_number_)
	{
		auto statsd_parser = [this](char* buffer) { return this->parse_statsd(buffer); };
		// Check for systemd socket activation for the statsd port
		auto systemd_statsd_fd = get_systemd_udp_socket(*statsd_port_number_);
		if (systemd_statsd_fd)
		{
			logger->info("Using systemd socket activation for statsd server on port {}/udp (fd={})",
			             *statsd_port_number_, *systemd_statsd_fd);
			bool is_ipv6 = is_socket_ipv6(*systemd_statsd_fd);
			statsd_server = std::make_unique<UdpServer>(io_context, *systemd_statsd_fd, is_ipv6, statsd_parser);
		}
		else
		{
			logger->info("Starting statsd server on port {}/udp (ipv4_only={})", *statsd_port_number_, ipv4_only_);
			statsd_server = std::make_unique<UdpServer>(io_context, ipv4_only_, *statsd_port_number_, statsd_parser);
		}
		statsd_server->Start();
	}
	else
	{
		logger->info("statsd support is not enabled");
	}

	std::unique_ptr<LocalServer> local_server;
	if (socket_path_)
	{
		prepare_socket_path(*socket_path_);
		local_server = std::make_unique<LocalServer>(io_context, *socket_path_, parser);
		logger->info("Starting local server (dgram) on socket {}", *socket_path_);
		local_server->Start();
	}
	else
	{
		logger->info("unix socket support is not enabled");
	}

	// Notify systemd that we're ready to accept connections
	if (sd_notify("READY=1")) {
	  logger->info("Sent READY=1 notification to systemd");
	}

	io_context.run();
}

// This watchdog is removed from the upkeep loop, because there are many instances
// in the environment which do not have configurations that allow them to publish
// metrics. We do not want to fill those disks with core files. The function remains
// in the codebase, so that we can rapidly re-enable it for debugging.
void Server::ensure_not_stuck()
{
	auto now = absl::GetCurrentTimeNanos();
	const auto& cfg = registry_->GetConfig();
	auto elapsed = absl::Nanoseconds(now - registry_->GetLastSuccessTime());
	auto seconds = absl::ToDoubleSeconds(elapsed);

	if (cfg.is_enabled() && seconds > 60)
	{
		logger_->error("Too long since we were able to send metrics successfully: {} > 60s. ABORTING.", seconds);
		abort();
	}

	logger_->debug("Last batch of metrics was sent successfully {} seconds ago", seconds);
}

// run our background tasks
void Server::upkeep()
{
	const auto& cfg = registry_->GetConfig();

	static auto timers_size_gauge =
	    registry_->GetGauge("spectatord.percentileCacheSize", spectator::Tags{{"id", "timer"}});
	static auto ds_size_gauge =
	    registry_->GetGauge("spectatord.percentileCacheSize", spectator::Tags{{"id", "dist-summary"}});
	static auto timers_expired_ctr =
	    registry_->GetCounter("spectatord.percentileExpired", spectator::Tags{{"id", "timer"}});
	static auto ds_expired_ctr =
	    registry_->GetCounter("spectatord.percentileExpired", spectator::Tags{{"id", "dist-summary"}});
	static auto sketch_size_gauge =
	    registry_->GetGauge("spectatord.sketchCacheSize", spectator::Tags{{"id", "distinct"}});
	static auto sketch_expired_ctr =
	    registry_->GetCounter("spectatord.sketchExpired", spectator::Tags{{"id", "distinct"}});

	static auto pool_hits = registry_->GetMonotonicCounter("spectatord.poolAccess", spectator::Tags{{"id", "hit"}});
	static auto pool_misses = registry_->GetMonotonicCounter("spectatord.poolAccess", spectator::Tags{{"id", "miss"}});
	static auto pool_alloc_size = registry_->GetGauge("spectatord.poolAllocSize");
	static auto pool_entries = registry_->GetGauge("spectatord.poolEntries");

	using clock = std::chrono::steady_clock;
	using std::chrono::duration_cast;
	using std::chrono::milliseconds;
	static constexpr auto kFrequency = milliseconds{30000};
	auto logger = Logger();
	size_t ds_size = 0;
	size_t ds_expired = 0;
	size_t t_size = 0;
	size_t t_expired = 0;
	size_t sketch_size = 0;
	size_t sketch_expired = 0;
	while (!should_stop_)
	{
		auto start = clock::now();

		std::tie(ds_size, ds_expired) = perc_ds_.expire();
		std::tie(t_size, t_expired) = perc_timers_.expire();
		std::tie(sketch_size, sketch_expired) = sketches_.expire();
		// Checked on every pass, so the cache is never left full and refusing
		// new ids for longer than one upkeep interval. The check itself is two
		// comparisons; clearing only happens when it is actually full.
		//
		// This is deliberately not done inline when put() finds the cache
		// full. Freeing the entries takes milliseconds, and on a host whose
		// ids are nearly all distinct the cache refills immediately, so
		// clearing on fill would stall the receive thread every fraction of a
		// second, risking dropped datagrams, on precisely the traffic for
		// which the cache is useless.
		if (id_cache_.clear_if_saturated())
		{
			logger->debug("id cache was full and has been cleared");
		}
		if (cfg.status_metrics_enabled)
		{
			timers_size_gauge->Set(t_size);
			ds_size_gauge->Set(ds_size);
			timers_expired_ctr->Add(t_expired);
			ds_expired_ctr->Add(ds_expired);
			sketch_size_gauge->Set(sketch_size);
			sketch_expired_ctr->Add(sketch_expired);
		}
#ifdef __linux__
		update_network_metrics();
#endif
		auto pool_stats = spectator::string_pool_stats();
		if (cfg.status_metrics_enabled)
		{
			pool_hits->Set(pool_stats.hits);
			pool_misses->Set(pool_stats.misses);
			pool_alloc_size->Set(pool_stats.alloc_size);
			pool_entries->Set(pool_stats.table_size);
		}
		logger_->debug("Str Pool: Hits {} Misses {} Size {} Alloc {}", pool_stats.hits, pool_stats.misses,
		               pool_stats.table_size, pool_stats.alloc_size);

		auto elapsed = clock::now() - start;
		auto millis = duration_cast<milliseconds>(elapsed);
		if (millis < kFrequency)
		{
			std::unique_lock<std::mutex> lock{cv_mutex_};
			auto sleep = kFrequency - elapsed;
			logger->debug("Janitor - sleeping {}ms", duration_cast<milliseconds>(sleep).count());
			cv_.wait_for(lock, sleep);
		}
	}
}

void Server::update_network_metrics()
{
	// parse /proc/net/udp to get dropped packets for our ports
	const auto& cfg = registry_->GetConfig();

	static auto udp_packets_dropped_ctr = registry_->GetMonotonicCounter("spectatord.udpPacketsDropped");
	static auto udp_queue = registry_->GetMaxGauge("spectatord.udpRxQueue");

	auto info = udp_info(port_number_);
	if (info && cfg.status_metrics_enabled)
	{
		udp_packets_dropped_ctr->Set(info->num_dropped);
		udp_queue->Set(info->rx_queue_bytes);
	}
}

void Server::Stop()
{
	if (!should_stop_.exchange(true))
	{
		logger_->debug("Stopping background tasks");
		cv_.notify_all();
		upkeep_thread_.join();
	}
}

auto Server::parse_statsd(char* buffer) -> std::optional<std::string>
{
	return parse_lines(buffer, [this](char* line) { return this->parse_statsd_line(line); });
}

auto Server::parse(char* buffer) -> std::optional<std::string>
{
	return parse_lines(buffer, [this](const char* line) { return this->parse_line(line); });
}

auto Server::parse_line(const char* buffer) -> std::optional<std::string>
{
	static int_fast64_t parsed_count = 0;

	auto fail = [buffer](const std::string& msg)
	{
		Logger()->info(fmt::format("Parse error for '{}': {}", buffer, msg));
		return msg;
	};

	const char* p = buffer;

	auto type = *p++;
	auto extra = int64_t{0};
	if (*p == ',')
	{
		++p;
		char* end_ttl = nullptr;
		extra = strtoll(p, &end_ttl, 10);
		if (extra <= 0)
		{
			auto idx = p - buffer;
			if (type == 'g')
			{
				return fail(fmt::format("Invalid ttl specified for gauge at index {}", idx));
			}
			else if (type == 'X')
			{
				return fail(fmt::format("Invalid timestamp specified for monotonic sampled source at index {}", idx));
			}
		}
		p = end_ttl;
	}
	if (*p != ':')
	{
		return fail(fmt::format("Expecting separator ':' at index {}", p - buffer));
	}
	++p;

	// The identity of the measurement (name and tags) precedes the value.
	// Repeated measurements for the same id are the common case, so the parsed
	// Id is cached by (type, id text) and a hit skips tag parsing and interning.
	const char* id_begin = p;
	const char* id_end = std::strchr(id_begin, ':');
	// A line is only cacheable when its id is terminated by a value separator,
	// which a successful parse does not imply: the sketch types accept an empty
	// value, so "S:a,z" parses even though it contains no second ':' at all and
	// the extent of its id is never established. An empty id needs no check
	// here, because get_measurement rejects it as "Missing name" before this
	// key could be used to cache anything.
	auto cacheable = id_end != nullptr;
	std::string_view id_key;
	std::shared_ptr<const spectator::Id> cached_id;
	if (cacheable)
	{
		id_key = std::string_view{id_begin, static_cast<size_t>(id_end - id_begin)};
		cached_id = id_cache_.get(type, id_key);
	}

	// On a hit the id was already validated when it was first cached, so only
	// the value is parsed. On a miss the whole measurement is parsed and
	// validated exactly as before. A line with no value separator never reaches
	// the hit path, since it is not cacheable, so it is always parsed in full.
	std::string err_msg;
	valueT value{};
	std::string_view str_value{};
	std::optional<measurement> parsed;
	if (cached_id != nullptr)
	{
		auto v = parse_value(type, std::string_view{id_end + 1}, &err_msg);
		if (!v)
		{
			return fail(err_msg);
		}
		value = v->value;
		str_value = v->str_value;
	}
	else
	{
		auto parsed_id_len = std::numeric_limits<size_t>::max();
		parsed = get_measurement(type, id_begin, &err_msg, &parsed_id_len);
		if (!parsed)
		{
			return fail(err_msg);
		}
		value = parsed->value;
		str_value = parsed->str_value;
		// Only cache when the id really ended at the first ':'. A tag key is
		// found with find('='), so it may contain a colon ("m,a:b=1:5" has the
		// id "m,a:b=1"), in which case the key would only be a prefix of the id
		// and a shorter line sharing that prefix would resolve to this meter.
		// An id that does end there is self-contained: every tag segment has its
		// '=' before the colon, so it parses the same way whatever follows.
		cacheable = cacheable && parsed_id_len == id_key.size();
	}

	if (!err_msg.empty())
	{
		// got a warning while parsing
		Logger()->info("While parsing {}: {}", p, err_msg);
	}

	// The meter is still looked up for every line; only the parsing and
	// interning that produced the Id is skipped. A cached Id is a value, never a
	// meter, so it cannot refer to a meter the registry has removed. The
	// percentile and sketch types resolve through expiring_cache rather than the
	// registry, which is unchanged by this: they are keyed by the same Id.
	if (cached_id == nullptr && cacheable)
	{
		cached_id = id_cache_.put(type, id_key, parsed->id);
	}
	const spectator::Id& rid = cached_id != nullptr ? *cached_id : parsed->id;

	switch (type)
	{
		case 'A':
		{
			auto gauge = registry_->GetAgeGauge(rid);
			if (value.d == 0)
			{
				gauge->UpdateLastSuccess();
			}
			else
			{
				gauge->UpdateLastSuccess(static_cast<int64_t>(value.d * 1e9));
			}
			break;
		}
		case 'c':
			registry_->GetCounter(rid)->Add(value.d);
			break;
		case 'C':
			registry_->GetMonotonicCounter(rid)->Set(value.d);
			break;
		case 'd':
			registry_->GetDistributionSummary(rid)->Record(value.d);
			break;
		case 'D':
			perc_ds_.get_or_create(registry_, rid)
			    ->Record(static_cast<int64_t>(value.d));
			break;
		case 'g':
		{
			// GetGauge(id, ttl) refreshes the ttl; without one the previous ttl
			// is preserved rather than overridden with the default
			auto gauge = extra > 0 ? registry_->GetGauge(rid, absl::Seconds(extra)) : registry_->GetGauge(rid);
			gauge->Set(value.d);
			break;
		}
		case 'm':
			registry_->GetMaxGauge(rid)->Update(value.d);
			break;
		case 'S':
		{
			// Distinct count sketch: the value is base64-encoded raw bytes that spectatord
			// hashes. This keeps the hash implementation in one place for all thin clients.
			std::string decoded;
			if (!absl::Base64Unescape(str_value, &decoded))
			{
				return fmt::format("Invalid base64 value for distinct count sketch: {}", str_value);
			}
			sketches_.get_or_create(registry_, rid)
			    ->Record(decoded.data(), decoded.size());
		}
		break;
		case 's':
			// Distinct count sketch with a precomputed xxHash64 (seed 0) value, for clients that
			// hash locally (e.g. to keep the raw value off the wire or bound the line size).
			sketches_.get_or_create(registry_, rid)->RecordHash(value.u);
			break;
		case 't':  // elapsed time is reported in seconds
		{
			auto nanos = static_cast<int64_t>(value.d * 1e9);
			registry_->GetTimer(rid)
			    ->Record(std::chrono::nanoseconds(nanos));
		}
		break;
		case 'T':
		{
			auto nanos = static_cast<int64_t>(value.d * 1e9);
			perc_timers_.get_or_create(registry_, rid)
			    ->Record(std::chrono::nanoseconds(nanos));
		}
		break;
		case 'U':
			registry_->GetMonotonicCounterUint(rid)->Set(value.u);
			break;
		case 'X':
			if (extra > 0)
			{
				// extra is milliseconds since the epoch
				auto nanos = extra * 1000 * 1000;
				registry_->GetMonotonicSampled(rid)->Set(value.u, nanos);
			}
			break;
		default:
			return fmt::format("Unknown type: {}", type);
	}

	++parsed_count;
	if (parsed_count % 50000 == 0)
	{
		logger_->debug("Parsed {} messages", parsed_count);
		logger_->debug("Meters in Registry = {}", registry_->Size());
	}
	return {};
}

}  // namespace spectatord
