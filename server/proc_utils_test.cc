#include "gtest/gtest.h"
#include "proc_utils.h"

namespace {
using spectatord::max_buffer_size;
using spectatord::udp_info;
using spectatord::udp_info_t;

TEST(ProcUtils, max_buffer) {
  // test repeated calls which should only read from the file once
  EXPECT_EQ(max_buffer_size("resources/rmem_max"), 65536);
  EXPECT_EQ(max_buffer_size("resources/rmem_max"), 65536);
  EXPECT_EQ(max_buffer_size("resources/rmem_max"), 65536);

  // get default when we can't read the file
  constexpr auto kDefault = 16777216;
  EXPECT_EQ(max_buffer_size("/foo/bar"), kDefault);
}

TEST(ProcUtils, udp_info) {
  auto res = udp_info(1234, "resources/net_udp_1");
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res->num_dropped, 166662);
  EXPECT_EQ(res->rx_queue_bytes, 0);

  res = udp_info(1235, "resources/net_udp_1");
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res->num_dropped, 10);
  EXPECT_EQ(res->rx_queue_bytes, 0x100);

  res = udp_info(1, "resources/net_udp_1");
  ASSERT_FALSE(res.has_value());

  res = udp_info(1234, "/does-not-exist-dir/does-not-exist-file");
  ASSERT_FALSE(res.has_value());
}
}  // namespace