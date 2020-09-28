#pragma once
#include "gzip.h"
#include <cassert>
#include <string_view>
#include <vector>

namespace spectator {

struct CompressedResult {
  const uint8_t* data;
  size_t size;
};

class CompressedBuffer {
 public:
  static constexpr size_t kDefaultChunkSize = 256 * 1024;
  static constexpr size_t kDefaultOutSize = 256 * 1024;
  explicit CompressedBuffer(size_t chunk_size = kDefaultChunkSize,
                            size_t out_size = kDefaultOutSize);
  CompressedBuffer(const CompressedBuffer&) = delete;
  CompressedBuffer(CompressedBuffer&&) = default;
  CompressedBuffer& operator=(const CompressedBuffer&) = delete;
  CompressedBuffer& operator=(CompressedBuffer&&) = delete;

  ~CompressedBuffer();

  void Init();

  void Append(std::string_view s) {
    assert(init_);
    std::copy(s.begin(), s.end(), std::back_inserter(cur_));
    maybe_compress();
  }

  void Append(uint8_t b) {
    assert(init_);
    cur_.push_back(b);
    maybe_compress();
  }

  void Append(uint8_t b1, uint8_t b2) {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    maybe_compress();
  }

  void Append(uint8_t b1, uint8_t b2, uint8_t b3) {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    cur_.push_back(b3);
    maybe_compress();
  }

  void Append(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    cur_.push_back(b3);
    cur_.push_back(b4);
    maybe_compress();
  }

  void Append(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5) {
    assert(init_);
    cur_.push_back(b1);
    cur_.push_back(b2);
    cur_.push_back(b3);
    cur_.push_back(b4);
    cur_.push_back(b5);
    maybe_compress();
  }

  void Append(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5,
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

  CompressedResult Result();

 private:
  bool init_{false};
  std::vector<uint8_t> cur_;
  size_t chunk_size_;
  std::vector<uint8_t> dest_;
  int dest_index_{0};
  z_stream stream;

  void compress(int flush);

  void maybe_compress();
};

}  // namespace spectator
