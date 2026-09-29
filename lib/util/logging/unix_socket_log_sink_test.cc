#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "unix_socket_log_sink.h"

namespace
{

class UnixSinkTest : public ::testing::Test {
   protected:
	void SetUp() override
	{
		unlink(socket_path_.c_str());
	}

	void TearDown() override
	{
		if (recv_fd_ >= 0)
		{
			close(recv_fd_);
		}
		unlink(socket_path_.c_str());
	}

	void create_receiver(int rcvbuf_size = 0)
	{
		recv_fd_ = socket(AF_UNIX, SOCK_DGRAM, 0);
		ASSERT_GE(recv_fd_, 0);

		sockaddr_un addr{};
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
		ASSERT_EQ(bind(recv_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

		if (rcvbuf_size > 0)
		{
			setsockopt(recv_fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size));
		}
	}

	std::string socket_path_ = "/tmp/spectatord_unix_sink_test.sock";
	int recv_fd_ = -1;
};

TEST_F(UnixSinkTest, SendsMessages)
{
	create_receiver();

	spdlog::sinks::unix_sink_config cfg{socket_path_};
	auto sink = std::make_shared<spdlog::sinks::unix_sink_mt>(cfg);
	auto logger = std::make_shared<spdlog::logger>("test", sink);

	logger->info("hello");
	logger->flush();

	char buf[1024];
	auto n = recv(recv_fd_, buf, sizeof(buf), 0);
	ASSERT_GT(n, 0);
	std::string received(buf, n);
	EXPECT_NE(received.find("hello"), std::string::npos);
}

// Verifies that buffer-full errors (EWOULDBLOCK on Linux, ENOBUFS on macOS)
// are silently dropped rather than propagated as spdlog exceptions. This is
// critical because the sink is non-blocking and used in an async logger —
// transient backpressure should never surface as an error.
TEST_F(UnixSinkTest, DropsMessagesWhenBufferFull)
{
	create_receiver(1);  // smallest possible receive buffer

	spdlog::sinks::unix_sink_config cfg{socket_path_};
	auto sink = std::make_shared<spdlog::sinks::unix_sink_mt>(cfg);
	auto logger = std::make_shared<spdlog::logger>("test_full", sink);

	int error_count = 0;
	logger->set_error_handler([&](const std::string&) { error_count++; });

	for (int i = 0; i < 10000; ++i)
	{
		logger->info("message {}", i);
	}
	logger->flush();

	EXPECT_EQ(error_count, 0);
}


}  // namespace
