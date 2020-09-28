#include <gtest/gtest.h>
#include <fmt/format.h>
#include "spectator/string_intern.h"

namespace {
using spectator::intern_str;
using spectator::StrRef;

TEST(StringIntern, Basic) {
  auto const_char = intern_str("foo");
  auto const_char2 = intern_str("foo");
  EXPECT_STREQ(const_char.Get(), const_char2.Get());
  EXPECT_EQ(const_char.Length(), const_char.Length());
  EXPECT_EQ(const_char, const_char2);

  auto strview = intern_str(std::string_view("foo"));
  auto strview2 = intern_str(std::string_view("bar"));

  EXPECT_EQ(strview, const_char);
  EXPECT_NE(strview2, strview);

  EXPECT_EQ(intern_str(std::string("bar")), strview2);
}

TEST(StringIntern, Valid) {
  // invalid chars get rewritten as _
  EXPECT_EQ(intern_str("foo_bar"), intern_str("foo@bar"));
  EXPECT_EQ(intern_str("foo#bar"), intern_str("foo!bar"));
  EXPECT_EQ(intern_str("abc\",abc"), intern_str("abc__abc"));
  std::string very_invalid = "12345";
  very_invalid[2] = static_cast<char>(-23);
  EXPECT_EQ(intern_str(very_invalid), intern_str("12_45"));
}

TEST(StringInter, Hash) {
  auto h1 = std::hash<spectator::StrRef>{}(intern_str("hash"));
  auto h2 = std::hash<spectator::StrRef>{}(intern_str("hash1"));
  auto h3 = std::hash<spectator::StrRef>{}(intern_str("hash2"));
  EXPECT_NE(h1, h2);
  EXPECT_NE(h2, h3);

  std::string s("hash2");
  EXPECT_EQ(std::hash<spectator::StrRef>{}(intern_str(s)), h3);
}
}  // namespace