#include "../spectator/strings.h"
#include <gtest/gtest.h>

namespace {

TEST(Strings, PathFromUrl) {
  using spectator::PathFromUrl;

  EXPECT_EQ(PathFromUrl("http://example.com:81/foo/bar?a=b"), "/foo/bar");

  EXPECT_EQ(PathFromUrl("/foo/bar"), "/foo/bar");
  EXPECT_EQ(PathFromUrl("http://foo"), "/");
  EXPECT_EQ(PathFromUrl("http:/"), "http:/");  // can't parse this
}

}  // namespace
