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

  // Small output sizes are to ensure there will not be enough space
  // to finish the compressed buffer and a resize will be required to
  // correctly compress the data.
  CompressedBuffer buf{1024, 32, 32};
  buf.Init();
  buf.Append(string1);
  buf.Append(string2);
  buf.Append(string3);
  buf.Append(static_cast<uint8_t>('a'), static_cast<uint8_t>('b'), static_cast<uint8_t>('c'));
  auto res{buf.Result()};

  char uncompressed[32768];
  size_t dest_len = sizeof(uncompressed);
  gzip_uncompress(uncompressed, &dest_len, res.data, res.size);

  std::string expected = absl::StrCat(string1, string2, string3, "abc");
  std::string result{uncompressed, dest_len};
  EXPECT_EQ(result, expected);

  // make sure we can reuse the buffer
  buf.Init();
  buf.Append(string2);
  buf.Append(static_cast<uint8_t>(':'), static_cast<uint8_t>('d'));
  res = buf.Result();

  gzip_uncompress(uncompressed, &dest_len, res.data, res.size);
  expected = absl::StrCat(string2, ":d");
  std::string result2{uncompressed, dest_len};
  EXPECT_EQ(result2, expected);
}

// This test is commented out intentionally. Uncomment this test to ensure the correct behavior
// of each static assert. This is to ensure specefic behavior for the templated Append
/*
TEST(CompressedBuffer, StaticAssertTest) {

  CompressedBuffer buf{1024, 32, 32};

  // Assert compile time failure of adding a double
  buf.Append(3.4);

  // Assert a compile time failure of adding nothing
  buf.Append();

  // Assert a compile time failure of adding more that one sting type
  buf.Append("Hello", "World");

  // Assert a compile time failure of passing a Vector to Append
  buf.Append(std::vector<int>());

  // Assert a compile time failure of adding more than 6 uint8_t types
  buf.Append(static_cast<uint8_t>(65), static_cast<uint8_t>(66), static_cast<uint8_t>(67),
            static_cast<uint8_t>(68), static_cast<uint8_t>(69), static_cast<uint8_t>(70),
            static_cast<uint8_t>(71));
}
*/

}  // namespace
