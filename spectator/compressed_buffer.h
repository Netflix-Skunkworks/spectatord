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
  auto operator=(CompressedBuffer&&) -> CompressedBuffer& = delete;

  ~CompressedBuffer();

  auto Init() -> void;

  /**
   * Append Templated Types to an internal buffer
   *
   * A variadic function that accepts a flexible number of arguments of either strings (std::string
   * or std::string_view) or (uint8_t). It checks that the types are valid at compile time, and
   * forwards them to an internal appending function for processing. We have statically asserted
   * that all types in the parameter pack are either string_view or uint8_t
   *
   * @param args is a parameter pack of forwarding references that can accept a variable number of
   * arguments of any type.
   *
   */
  template <typename... Args>
  auto Append(Args&&... args) -> void {
    static_assert(
        ((std::is_convertible_v<std::decay_t<Args>, std::string_view> ||
          std::is_same_v<std::decay_t<Args>, uint8_t>) &&
         ...),
        "Invalid argument types. Only std::string, std::string_view, or uint8_t are allowed.");
    assert(init_);
    AppendImpl(std::forward<Args>(args)...);
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

  template <typename... Args>
  auto AppendImpl(Args&&... args) -> void {
    if constexpr ((std::is_convertible_v<std::decay_t<Args>, std::string_view> && ...)) {
      static_assert(sizeof...(args) == 1, "Only one string-like argument is allowed.");
      (cur_.insert(cur_.end(), std::forward<Args>(args).begin(), std::forward<Args>(args).end()),
       ...);
    } else if constexpr ((std::is_same_v<std::decay_t<Args>, uint8_t> && ...)) {
      static_assert(sizeof...(args) >= 1 && sizeof...(args) <= 6,
                    "The number of uint8_t arguments must be between 1 and 6.");
      (cur_.push_back(std::forward<Args>(args)), ...);
    }
  }
};

}  // namespace spectator
