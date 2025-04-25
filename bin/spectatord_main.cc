#include "../admin/admin_server.h"
#include "../server/spectatord.h"
#include "../spectator/version.h"
#include "absl/flags/flag.h"
#include "absl/flags/internal/program_name.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/flags/usage_config.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "backward.hpp"

#include <cstdlib>
#include <fmt/ranges.h>

auto GetSpectatorConfig() -> std::unique_ptr<spectator::Config>;

using spectatord::GetLogger;
using spectatord::Logger;

struct PortNumber {
  explicit PortNumber(int p = 0) : port(p) {}
  int port;  // Valid range is [1..65535]
};

// Returns a textual flag value corresponding to the PortNumber `p`.
auto AbslUnparseFlag(PortNumber p) -> std::string {
  // Delegate to the usual unparsing for int.
  return absl::UnparseFlag(p.port);
}

// Parses a PortNumber from the command line flag value `text`.
// Returns true and sets `*p` on success; returns false and sets `*error`
// on failure.
auto AbslParseFlag(absl::string_view text, PortNumber* p, std::string* error) -> bool {
  // Convert from text to int using the int-flag parser.
  if (!absl::ParseFlag(text, &p->port, error)) {
    return false;
  }
  if (p->port < 1 || p->port > 65535) {
    *error = "not in range [1,65535]";
    return false;
  }
  return true;
}

ABSL_FLAG(PortNumber, admin_port, PortNumber(1234),
          "Port number for the admin server.");
ABSL_FLAG(size_t, age_gauge_limit, 1000,
          "The maximum number of age gauges that may be reported by this process.");
ABSL_FLAG(std::string, common_tags, "",
          "Common tags: nf.app=app,nf.cluster=cluster. Override the default common "
          "tags. If empty, then spectatord will use the default set. "
          "This flag should only be used by experts who understand the risks.");
ABSL_FLAG(bool, debug, false,
          "Debug spectatord. All values will be sent to a dev aggregator and "
          "dropped.");
ABSL_FLAG(bool, enable_external, false,
          "Enable external publishing.");
#ifdef __linux__
ABSL_FLAG(bool, enable_socket, true,
          "Enable UNIX domain socket support. Default is true on Linux and false "
          "on MacOS and Windows.");
#else
ABSL_FLAG(bool, enable_socket, false,
          "Enable UNIX domain socket support. Default is true on Linux and false "
          "on MacOS and Windows.");
#endif
ABSL_FLAG(bool, enable_statsd, false,
          "Enable statsd support.");
ABSL_FLAG(bool, ipv4_only, false,
          "Enable IPv4-only UDP listeners. This option should only be used in environments "
          "where it is impossible to run IPv6.");
ABSL_FLAG(std::string, metatron_dir, "",
          "Path to the Metatron certificates, which are used for external publishing. A number "
          "of well-known directories are searched by default. This option is only necessary "
          "if your certificates are in an unusual location.");
ABSL_FLAG(absl::Duration, meter_ttl, absl::Minutes(15),
          "Meter TTL: expire meters after this period of inactivity.");
ABSL_FLAG(bool, no_common_tags, false,
          "No common tags will be provided for metrics. Since no common tags are available, no "
          "internal status metrics will be recorded. Only use this feature for special cases "
          "where it is absolutely necessary to override common tags such as nf.app, and only "
          "use it with a secondary spectatord process.");
ABSL_FLAG(PortNumber, port, PortNumber(1234),
          "Port number for the UDP socket.");
ABSL_FLAG(std::string, process_name, "spectatord",
          "The nf.process tag value that will be added to internal status metrics. We do not "
          "set the corresponding environment value, because only the internal status metrics "
          "should have this tag, and all other metrics should be exempt.");
ABSL_FLAG(std::string, socket_path, "/run/spectatord/spectatord.unix",
          "Path to the UNIX domain socket.");
ABSL_FLAG(PortNumber, statsd_port, PortNumber(8125),
          "Port number for the statsd socket.");
ABSL_FLAG(std::string, uri, "",
          "Optional override URI for the aggregator.");
ABSL_FLAG(bool, verbose, false,
          "Use verbose logging.");
ABSL_FLAG(bool, verbose_http, false,
          "Output debug info for HTTP requests.");

auto main(int argc, char** argv) -> int {
  auto logger = Logger();

  auto signals = backward::SignalHandling::make_default_signals();
  // default signals except SIGABRT
  signals.erase(std::remove(signals.begin(), signals.end(), SIGABRT), signals.end());
  backward::SignalHandling sh{signals};

  auto usage_config = absl::flags_internal::GetUsageConfig();
  usage_config.version_string = []() { return fmt::format("spectatord/{}\n", VERSION); };
  absl::SetFlagsUsageConfig(usage_config);
  absl::SetProgramUsageMessage("A daemon that listens for metrics and reports them to Atlas.");
  absl::ParseCommandLine(argc, argv);

  auto cfg = GetSpectatorConfig();

  cfg->process_name = absl::GetFlag(FLAGS_process_name);

  if (absl::GetFlag(FLAGS_enable_external)) {
    cfg->external_enabled = true;
    cfg->metatron_dir = absl::GetFlag(FLAGS_metatron_dir);
    // provide a reasonable default for external publishing
    cfg->uri = cfg->external_uri;
  }

  if (!absl::GetFlag(FLAGS_uri).empty()) {
    // allow overriding the uri for both standard and external publishing
    cfg->uri = absl::GetFlag(FLAGS_uri);
  }

  if (absl::GetFlag(FLAGS_debug)) {
    // when running in debug mode, never publish externally
    cfg->external_enabled = false;
    cfg->uri = "https://atlas-aggr-dev.us-east-1.ieptest.netflix.net/api/v4/update";
  }

  if (absl::GetFlag(FLAGS_verbose_http)) {
    cfg->verbose_http = true;
  }

  cfg->meter_ttl = absl::GetFlag(FLAGS_meter_ttl);

  cfg->age_gauge_limit = absl::GetFlag(FLAGS_age_gauge_limit);

  auto spectator_logger = GetLogger("spectator");
  if (absl::GetFlag(FLAGS_verbose)) {
    logger->set_level(spdlog::level::trace);
    spectator_logger->set_level(spdlog::level::trace);
  } else {
    logger->set_level(spdlog::level::info);
    spectator_logger->set_level(spdlog::level::info);
  }

  auto maybe_common_tags = absl::GetFlag(FLAGS_common_tags);
  if (!maybe_common_tags.empty()) {
    std::map<std::string, std::string> common_tags;
    for (auto sp : absl::StrSplit(maybe_common_tags, ',')) {
      std::vector<std::string> kv = absl::StrSplit(sp, absl::MaxSplits('=', 1));
      if (kv.size() != 2 || kv.at(0).empty() || kv.at(1).empty()) {
        logger->error("Invalid common tags specified: {}", maybe_common_tags);
        exit(EXIT_FAILURE);
      }
      common_tags[kv.at(0)] = kv.at(1);
    }
    logger->info("Using common tags: {}", common_tags);
    cfg->common_tags = std::move(common_tags);
  }

  if (absl::GetFlag(FLAGS_no_common_tags)) {
    cfg->common_tags.clear();
    cfg->status_metrics_enabled = false;
  }

  if (!sh.loaded()) {
    logger->info("Unable to load signal handling for stacktraces");
  }

  spectator::Registry registry{std::move(cfg), std::move(spectator_logger)};
  registry.Start();

  std::optional<std::string> socket_path;
  if (absl::GetFlag(FLAGS_enable_socket)) {
    socket_path = absl::GetFlag(FLAGS_socket_path);
  }

  std::optional<int> statsd_port;
  if (absl::GetFlag(FLAGS_enable_statsd)) {
    statsd_port = absl::GetFlag(FLAGS_statsd_port).port;
  }

  logger->info("Starting admin server on port {}/tcp", absl::GetFlag(FLAGS_admin_port).port);
  admin::AdminServer admin_server(registry, absl::GetFlag(FLAGS_admin_port).port);
  admin_server.Start();

  spectatord::Server server{absl::GetFlag(FLAGS_ipv4_only), absl::GetFlag(FLAGS_port).port,
                            statsd_port, socket_path, &registry};
  server.Start();

  return 0;
}
