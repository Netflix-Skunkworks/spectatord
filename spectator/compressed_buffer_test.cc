#include <gtest/gtest.h>

#include "compressed_buffer.h"
#include "gzip.h"
#include "absl/strings/str_cat.h"

namespace {

using spectator::CompressedBuffer;
using spectator::gzip_uncompress;

TEST(CompressedBuffer, Basic) {
  std::string string1(1000, 'a');
  std::string string2(1000, 'b');
  std::string string3(1000, 'c');

  // start with a tiny out to test the resize
  CompressedBuffer buf{1024, 32};
  buf.Init();
  buf.Append(string1);
  buf.Append(string2);
  buf.Append(string3);
  buf.Append('a', 'b', 'c');
  auto res = buf.Result();

  char uncompressed[32768];
  size_t dest_len = sizeof(uncompressed);
  gzip_uncompress(uncompressed, &dest_len, res.data, res.size);

  std::string expected = absl::StrCat(string1, string2, string3, "abc");
  std::string result{uncompressed, dest_len};
  EXPECT_EQ(result, expected);

  // make sure we can reuse the buffer
  buf.Init();
  buf.Append(string2);
  buf.Append(':', 'd');
  res = buf.Result();

  gzip_uncompress(uncompressed, &dest_len, res.data, res.size);
  expected = absl::StrCat(string2, ":d");
  std::string result2{uncompressed, dest_len};
  EXPECT_EQ(result2, expected);
}

}  // namespace
