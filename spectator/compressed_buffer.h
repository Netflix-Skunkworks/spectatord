#pragma once

#include "gzip.h"
#include <cassert>
#include <cstdint>
#include <string_view>
#include <vector>

namespace spectator {

struct CompressedResult {
  const uint8_t* data;
  size_t size;
};

class CompressedBuffer {
 public:
  static constexpr size_t kDefaultChunkSizeInput = 256 * 1024;
  static constexpr size_t kDefaultOutSize = 256 * 1024;
  static constexpr size_t kDefaultChunkSizeOutput = 32 * 1024;
  explicit CompressedBuffer(size_t chunk_size_input = kDefaultChunkSizeInput,
                            size_t out_size = kDefaultOutSize,
                            size_t chunk_size_output = kDefaultChunkSizeOutput);
  CompressedBuffer(const CompressedBuffer&) = delete;
  CompressedBuffer(CompressedBuffer&&) = default;
  auto operator=(const CompressedBuffer&) -> CompressedBuffer& = delete;
  auto operator=(CompressedBuffer &&) -> CompressedBuffer& = delete;

  ~CompressedBuffer();

  auto Init() -> void;

  auto Append(std::string_view s) -> void {
    assert(init_);
    std::copy(s.begin(), s.end(), std::back_inserter(cur_));
    maybe_compress();
  }

  auto Append(uint8_t b) -> void {
    assert(init_);
    cur_.push_back(b);
    maybe_compress();
  }

  auto Append(uint8_t b1, uint8_t b2) -> void {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    maybe_compress();
  }

  auto Append(uint8_t b1, uint8_t b2, uint8_t b3) -> void {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    cur_.push_back(b3);
    maybe_compress();
  }

  auto Append(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) -> void {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    cur_.push_back(b3);
    cur_.push_back(b4);
    maybe_compress();
  }

  auto Append(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
      -> void {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    cur_.push_back(b3);
    cur_.push_back(b4);
    cur_.push_back(b5);
    maybe_compress();
  }

  auto Append(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5,
              uint8_t b6) {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    cur_.push_back(b3);
    cur_.push_back(b4);
    cur_.push_back(b5);
    cur_.push_back(b6);
    maybe_compress();
  }

  auto Result() -> CompressedResult;

 private:
  bool init_{false};
  std::vector<uint8_t> cur_;
  size_t chunk_size_input_;
  std::vector<uint8_t> dest_;
  size_t chunk_size_output_;
  int dest_index_{0};
  z_stream stream;

  auto compress(int flush) -> void;

  auto maybe_compress() -> void;

  auto deflate_chunk(size_t chunk_size, int flush) -> int;
};

}  // namespace spectator
