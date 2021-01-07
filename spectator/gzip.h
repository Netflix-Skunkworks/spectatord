#pragma once

#include <string>
#include <zlib.h>

namespace spectator {

static constexpr int kGzipHeaderSize = 16;  // size of the gzip header
static constexpr int kWindowBits =
    15 + kGzipHeaderSize;            // 32k window (max size)
static constexpr int kMemLevel = 9;  // max memory for optimal speed

auto gzip_compress(void* dest, size_t* destLen, const void* source,
                   size_t sourceLen) -> int;
auto gzip_uncompress(void* dest, size_t* destLen, const void* source,
                     size_t sourceLen) -> int;

}  // namespace spectator
