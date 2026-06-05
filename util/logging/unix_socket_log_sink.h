#pragma once

#include <iostream>
#include <string>

#include <asio.hpp>
#include <spdlog/common.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

// Unix domain socket sink using ASIO for socket management.
// Sends formatted log lines over a non-blocking Unix datagram socket.
// Messages are dropped if the receiver buffer is full (never blocks).
// Connectionless — each send is independent with no state to manage.

namespace spdlog {
namespace sinks {

struct unix_sink_config {
	std::string path;
	spdlog::sink_ptr fallback;

	explicit unix_sink_config(std::string socket_path, spdlog::sink_ptr fallback_sink = nullptr)
		: path{std::move(socket_path)}, fallback{std::move(fallback_sink)}
	{
	}
};

template <typename Mutex>
class unix_sink : public spdlog::sinks::base_sink<Mutex> {
   public:
	explicit unix_sink(unix_sink_config sink_config)
		: config_{std::move(sink_config)},
		  fallback_{config_.fallback},
		  endpoint_{config_.path}
	{
		asio::error_code ec;
		socket_.open(asio::local::datagram_protocol(), ec);
		if (ec)
		{
			throw_spdlog_ex("unix_sink: open failed: " + ec.message());
		}
		socket_.non_blocking(true, ec);
		if (ec)
		{
			throw_spdlog_ex("unix_sink: non_blocking failed: " + ec.message());
		}
	}

	~unix_sink() override = default;

   protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		spdlog::memory_buf_t formatted;
		spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

		asio::error_code ec;
		socket_.send_to(asio::buffer(formatted.data(), formatted.size()), endpoint_, 0, ec);
		// Linux returns would_block when the receiver buffer is full;
		// macOS returns no_buffer_space (ENOBUFS) instead. Both are
		// transient and safe to drop silently.
		if (ec && ec != asio::error::would_block
		       && ec != asio::error::no_buffer_space)
		{
			// OTel Collector early socket is unreachable. Log the error to stderr
			// (visible in journald) and route this message through the TCP
			// fallback sink at 127.0.0.1:1552.
			if (fallback_)
			{
				std::string warn_text = "unix_sink: OTel Collector socket is down (" + ec.message() + "), falling back to TCP";
				spdlog::details::log_msg warn_msg(spdlog::source_loc{}, msg.logger_name, spdlog::level::err, warn_text);
				std::cerr << warn_text << std::endl;
				fallback_->log(warn_msg);
				fallback_->log(msg);
			}
			else
			{
				throw_spdlog_ex("unix_sink: send_to failed: " + ec.message());
			}
		}
	}
 
	void flush_() override {}

   private:
	unix_sink_config config_;
	spdlog::sink_ptr fallback_;
	asio::io_context io_context_;
	asio::local::datagram_protocol::socket socket_{io_context_};
	asio::local::datagram_protocol::endpoint endpoint_;
};

using unix_sink_mt = unix_sink<std::mutex>;
using unix_sink_st = unix_sink<spdlog::details::null_mutex>;

}  // namespace sinks
}  // namespace spdlog
