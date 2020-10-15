#include "backward.hpp"
#include "local.h"
#include "logger.h"
#include "spectatord.h"
#include "spectator/registry.h"
#include <cstdlib>
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include <fmt/ranges.h>

std::unique_ptr<spectator::Config> GetSpectatorConfig();

using spectatord::GetLogger;
using spectatord::Logger;

int get_port_number(spdlog::logger* logger, const char* str) {
  int n;
  if (!absl::SimpleAtoi(str, &n) || n < 0 || n > 65535) {
    logger->warn(
        "Invalid port number specified: {} - valid range is 1-65535. ");
    return -1;
  }
  return n;
}

void usage(int exit_code, const char* progname, spdlog::logger* logger) {
  logger->info("Usage: {} [options]", progname);
  logger->info("\t-p <port-number>\tPort number to use. Default 1234");
  logger->info("\t-s <port-number>\tStatsd Port number to use. Default 8125");
  logger->info(
      "\t-l <socket-path>\tPath for our unix domain socket. Default {}",
      spectatord::kSocketNameDgram);
  logger->info(
      "\t-t <ttl-seconds>\tExpire meters after ttl. Default 15 minutes");
  logger->info("\t-v\t\tVerbose logging");
  logger->info("\t-d\t\tDebug - send all meters to dev aggr to be dropped");
  logger->info("\t-h\t\tPrint this message");
  exit(exit_code);
}

struct PortNumber {
  explicit PortNumber(int p = 0) : port(p) {}
  int port;  // Valid range is [1..65535]
};

// Returns a textual flag value corresponding to the PortNumber `p`.
std::string AbslUnparseFlag(PortNumber p) {
  // Delegate to the usual unparsing for int.
  return absl::UnparseFlag(p.port);
}

// Parses a PortNumber from the command line flag value `text`.
// Returns true and sets `*p` on success; returns false and sets `*error`
// on failure.
bool AbslParseFlag(absl::string_view text, PortNumber* p, std::string* error) {
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

ABSL_FLAG(bool, debug, false,
          "Debug spectatord. All values will be sent to a dev aggregator and "
          "dropped.");
ABSL_FLAG(bool, verbose, false, "Use verbose logging");
ABSL_FLAG(bool, enable_statsd, false, "Enable statsd support");
ABSL_FLAG(absl::Duration, meter_ttl, absl::Minutes(15),
          "Meter ttl: expire meters after this period of inactivity");
ABSL_FLAG(std::string, socket_path, "/run/spectatord/spectatord.unix",
          "Path for our UNIX domain socket");
ABSL_FLAG(std::string, common_tags, "",
          "Common tags: nf.app=app,nf.cluster=cluster. Override default common "
          "tags. If left empty then spectatord will use the default set. "
          "It is discouraged to use this flag since it can cause confusion. "
          "Only for expert users who understand the risks");
ABSL_FLAG(PortNumber, port, PortNumber(1234), "Port number for our daemon");
ABSL_FLAG(PortNumber, statsd_port, PortNumber(8125),
          "Port number for statsd compatibility");

int main(int argc, char** argv) {
  auto logger = Logger();
  backward::SignalHandling sh;

  absl::SetProgramUsageMessage(
      "A daemon that listens for metrics and reports them to atlas");
  absl::ParseCommandLine(argc, argv);

  auto cfg = GetSpectatorConfig();
  if (absl::GetFlag(FLAGS_debug)) {
    cfg->uri =
        "http://atlas-aggr-dev.us-east-1.ieptest.netflix.net/api/v4/update";
  }

  cfg->meter_ttl = absl::GetFlag(FLAGS_meter_ttl);
  auto spectator_logger = GetLogger("spectator");
  if (absl::GetFlag(FLAGS_verbose)) {
    logger->set_level(spdlog::level::debug);
    spectator_logger->set_level(spdlog::level::debug);
  } else {
    logger->set_level(spdlog::level::info);
    spectator_logger->set_level(spdlog::level::info);
  }

  auto maybe_common_tags = absl::GetFlag(FLAGS_common_tags);
  if (!maybe_common_tags.empty()) {
    std::map<std::string, std::string> common_tags;
    for (std::string_view sp : absl::StrSplit(maybe_common_tags, ',')) {
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

  if (!sh.loaded()) {
    logger->info("Unable to load signal handling for stacktraces");
  }
  spectator::Registry registry{std::move(cfg), std::move(spectator_logger)};
  registry.Start();
  auto statsd_enabled = absl::GetFlag(FLAGS_enable_statsd);
  auto statsd_port =
      statsd_enabled ? absl::GetFlag(FLAGS_statsd_port).port : -1;
  spectatord::Server server{absl::GetFlag(FLAGS_port).port, statsd_port,
                            absl::GetFlag(FLAGS_socket_path), &registry};
  server.Start();
  return 0;
}
