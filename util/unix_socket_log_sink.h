#pragma once

#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <spdlog/common.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

// Unix domain socket sink following the same pattern as spdlog's tcp_sink / tcp_client.
// Connects to a Unix stream socket and sends formatted log lines.
// Will attempt to reconnect if the connection drops.

namespace spdlog {
namespace details {

class unix_client {
	int socket_ = -1;

   public:
	bool is_connected() const { return socket_ >= 0; }

	void close()
	{
		if (is_connected())
		{
			::close(socket_);
			socket_ = -1;
		}
	}

	int fd() const { return socket_; }

	~unix_client() { close(); }

	void connect(const std::string& path)
	{
		close();

		#if defined(SOCK_CLOEXEC)
			const int flags = SOCK_CLOEXEC;
		#else
			const int flags = 0;
		#endif
		
		socket_ = ::socket(AF_UNIX, SOCK_STREAM | flags, 0);
		if (socket_ < 0)
		{
			throw_spdlog_ex("unix_client: socket(2) failed", errno);
		}

		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

		if (::connect(socket_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
		{
			auto saved_errno = errno;
			::close(socket_);
			socket_ = -1;
			throw_spdlog_ex("unix_client: connect(2) failed to " + path, saved_errno);
		}

		// prevent sigpipe on systems where MSG_NOSIGNAL is not available
		#if defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
			int enable_flag = 1;
			::setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<char*>(&enable_flag),
		             sizeof(enable_flag));
		#endif

		#if !defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
		#error "unix_socket_sink would raise SIGPIPE since neither SO_NOSIGPIPE nor MSG_NOSIGNAL are available"
		#endif
	}

	void send(const char* data, size_t n_bytes)
	{
		size_t bytes_sent = 0;
		while (bytes_sent < n_bytes)
		{
			#if defined(MSG_NOSIGNAL)
				const int send_flags = MSG_NOSIGNAL;
			#else
				const int send_flags = 0;
			#endif
			auto write_result = ::send(socket_, data + bytes_sent, n_bytes - bytes_sent, send_flags);
			if (write_result < 0)
			{
				close();
				throw_spdlog_ex("unix_client: write(2) failed", errno);
			}
			if (write_result == 0)
			{
				break;
			}
			bytes_sent += static_cast<size_t>(write_result);
		}
	}
};

}  // namespace details

namespace sinks {

struct unix_sink_config {
	std::string path;
	bool lazy_connect = false;

	explicit unix_sink_config(std::string socket_path)
		: path{std::move(socket_path)}
	{
	}
};

template <typename Mutex>
class unix_sink : public spdlog::sinks::base_sink<Mutex> {
   public:
	explicit unix_sink(unix_sink_config sink_config)
		: config_{std::move(sink_config)}
	{
		if (!config_.lazy_connect)
		{
			client_.connect(config_.path);
		}
	}

	~unix_sink() override = default;

   protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		spdlog::memory_buf_t formatted;
		spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
		if (!client_.is_connected())
		{
			client_.connect(config_.path);
		}
		client_.send(formatted.data(), formatted.size());
	}

	void flush_() override {}

	unix_sink_config config_;
	details::unix_client client_;
};

using unix_sink_mt = unix_sink<std::mutex>;
using unix_sink_st = unix_sink<spdlog::details::null_mutex>;

}  // namespace sinks
}  // namespace spdlog

