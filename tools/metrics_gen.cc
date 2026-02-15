#include "../server/local.h"
#include "absl/strings/numbers.h"
#include <asio.hpp>
#include <fmt/format.h>
#include <memory>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

using metrics_t = std::vector<std::string>;
using asio::ip::udp;
using asio::local::stream_protocol;

class throttler_t
{
   public:
	explicit throttler_t(int rps)
	    : last_{std::chrono::high_resolution_clock::now()},
	      nanos_for_1k_{std::chrono::nanoseconds{1'000'000'000} / (rps / 1000)}
	{
	}
	void throttle(int i)
	{
		using std::chrono::high_resolution_clock;
		if (i % 1000 == 0)
		{
			auto elapsed = high_resolution_clock::now() - last_;
			auto nanos_to_sleep = nanos_for_1k_ - elapsed;
			if (nanos_to_sleep.count() > 0)
			{
				std::this_thread::sleep_for(nanos_to_sleep);
				last_ += nanos_for_1k_;
			}
			else
			{
				// running behind
				last_ = high_resolution_clock::now();
			}
		}
	}

   private:
	std::chrono::high_resolution_clock::time_point last_;
	std::chrono::nanoseconds nanos_for_1k_;
};

// rps in multiples of 1k
void udp_send_metrics(const metrics_t& metrics, int batch_size, spdlog::logger* logger, int port_number, int rps)
{
	asio::io_context io_context;
	using std::chrono::duration_cast;
	using std::chrono::high_resolution_clock;
	using std::chrono::milliseconds;

	throttler_t throttler{rps};
	udp::socket socket{io_context};
	udp::endpoint endpoint{asio::ip::address_v4::loopback(), static_cast<unsigned short>(port_number)};
	socket.open(udp::v4());
	socket.connect(endpoint);

	auto start = high_resolution_clock::now();
	auto last = start;
	auto i = 0;
	auto updateEvery = std::min(100000, rps * 2);
	logger->info("UDP {} metrics at {}k rps", metrics.size(), rps / 1000);
	for (const auto& m : metrics)
	{
		i += batch_size;
		throttler.throttle(i);
		socket.send(asio::buffer(m));

		if (i % updateEvery == 0)
		{
			auto now = high_resolution_clock::now();
			auto elapsed = now - last;
			auto millis = duration_cast<milliseconds>(elapsed);
			logger->info("Sent {} metrics in {}ms", i, millis.count());
			last = now;
			i = 0;
		}
	}

	auto elapsed = high_resolution_clock::now() - start;
	auto millis = duration_cast<milliseconds>(elapsed);
	logger->info("Sent {} metrics in {}s\n", metrics.size() * batch_size, millis.count() / 1000.0);
}

void local_send_metrics(const metrics_t& metrics, int batch_size, spdlog::logger* logger, int rps)
{
	asio::io_context io_context;
	using std::chrono::duration_cast;
	using std::chrono::high_resolution_clock;
	using std::chrono::milliseconds;
	auto updateEvery = std::min(100000, rps * 2);
	logger->info("Local dgram - {} metrics at {}k rps", metrics.size(), rps / 1000);
	throttler_t throttler{rps};

	std::string client_endpoint = fmt::format("{}.{}", spectatord::kSocketNameDgram, getpid());
	using endpoint_t = asio::local::datagram_protocol::endpoint;
	asio::local::datagram_protocol::socket sock{io_context, endpoint_t{client_endpoint}};
	sock.connect(endpoint_t{spectatord::kSocketNameDgram});

	auto start = high_resolution_clock::now();
	auto last = start;
	auto i = 0;
	for (const auto& m : metrics)
	{
		i += batch_size;
		sock.send(asio::buffer(m));
		throttler.throttle(i);
		if (i % updateEvery == 0)
		{
			auto now = high_resolution_clock::now();
			auto elapsed = now - last;
			auto millis = duration_cast<milliseconds>(elapsed);
			logger->info("Sent {} metrics in {}ms", i, millis.count());
			last = now;
			i = 0;
		}
	}

	auto elapsed = high_resolution_clock::now() - start;
	auto millis = duration_cast<milliseconds>(elapsed);
	logger->info("Sent {} metrics in {}s\n", metrics.size() * batch_size, millis.count() / 1000.0);
}

metrics_t batch(const metrics_t& metrics, int batch_size)
{
	if (batch_size == 1)
	{
		return metrics;
	}

	metrics_t result;
	result.reserve((metrics.size() + batch_size - 1) / batch_size);
	auto it = metrics.begin();
	while (it != metrics.end())
	{
		std::string s;
		for (auto i = 0; i < batch_size; ++i)
		{
			s = s.append(*it);
			++it;
			if (it == metrics.end())
			{
				break;
			}
		}
		result.emplace_back(std::move(s));
	}
	return result;
}

metrics_t gen_metrics(const char* prefix, int num, int distinct, int batch_size)
{
	metrics_t raw{};
	raw.reserve(num);
	for (auto i = 0; i < num / 10; ++i)
	{
		auto n = i % distinct;
		raw.emplace_back(fmt::format("c:spectatord_test.counter,id={}{},foo=some-foo:42.0\n", prefix, n));
		raw.emplace_back(
		    fmt::format("1:C:spectatord_test.monocounter,id={}{},foo=some-foo,bar="
		                "some-bar:{}\n",
		                prefix, n, i));
		raw.emplace_back(fmt::format("t:spectatord_test.timer,id={}{},foo=some-foo:0.5\n", prefix, n));
		raw.emplace_back(fmt::format("d:spectatord_test.ds,id={}{},foo=some-foo:42\n", prefix, n));
		raw.emplace_back(fmt::format("m:spectatord_test.max,id={}{},foo=some-foo:{}\n", prefix, n, i));
		raw.emplace_back(
		    fmt::format("D:spectatord_test.percDs,id={}{},tag1=some-tag-string,"
		                "tag2=some-value:{}\n",
		                prefix, n, n % 5));
		raw.emplace_back(
		    fmt::format("T:spectatord_test.percTimer,id={}{},invalid=some-"
		                "invalid-string:{}\n",
		                prefix, n, n % 5));
		raw.emplace_back(
		    fmt::format("c:spectatord_test.ctr,id={}{},nf.invalid=some-invalid-string:{}\n", prefix, n, i % 5));
		raw.emplace_back(fmt::format("c:spectatord_test.ctr2,id={}{},foo=bar,bar=baz:{}\n", prefix, n, i % 5));
		raw.emplace_back(fmt::format("g,30:spectatord_test.gauge,id={}{},foo=bar,bar=baz:{}\n", prefix, n, i % 5));
	}
	return batch(raw, batch_size);
}

void fatal(spdlog::logger* logger, const std::string& msg)
{
	logger->error(msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
	auto logger = spdlog::create_async_nb<spdlog::sinks::ansicolor_stdout_sink_mt>("metrics_gen");

	auto local = false;
	auto rps = 10000;
	auto port_number = 1234;
	auto batch_size = 1;

	int ch, n;
	while ((ch = getopt(argc, argv, "p:lur:b:")) != -1)
	{
		switch (ch)
		{
			case 'l':
				local = true;
				break;
			case 'u':
				// default is udp
				break;
			case 'b':
				if (absl::SimpleAtoi(optarg, &n) && n > 0)
				{
					batch_size = n;
				}
				break;
			case 'p':
				if (absl::SimpleAtoi(optarg, &n) && n > 0 && n < 65536)
				{
					port_number = n;
				}
				break;
			case 'r':
				if (!absl::SimpleAtoi(optarg, &n) || n <= 0)
				{
					fatal(logger.get(), fmt::format("Invalid rps: {}", optarg));
				}
				if (n < 1000)
				{
					logger->warn("Setting rps to 1000");
					n = 1000;
				}
				rps = n;
				break;
			default:
				logger->error(
				    "Usage: {} [options]\n"
				    "\twhere options are\n"
				    "\t-b <batch-size> (default 1)\n"
				    "\t-p <port-number> (default 1234)\n"
				    "\t-r <rps> (default 10000)\n"
				    "\t-l local (unix domain socket)\n"
				    "\t-u udp (default)\n",
				    argv[0]);
				exit(1);
		}
	}

	const char* prefix = "id";
	auto num_metrics = std::min(rps * 10, 10'000'000);
	auto metrics = gen_metrics(prefix, num_metrics, 100'000, batch_size);
	if (local)
	{
		local_send_metrics(metrics, batch_size, logger.get(), rps);
	}
	else
	{
		udp_send_metrics(metrics, batch_size, logger.get(), port_number, rps);
	}
}
