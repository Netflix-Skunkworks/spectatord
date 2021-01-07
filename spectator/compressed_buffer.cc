#include "compressed_buffer.h"
#include <fmt/format.h>

#ifndef z_const
#define z_const
#endif

namespace spectator {

CompressedBuffer::CompressedBuffer(size_t chunk_size, size_t out_size)
    : chunk_size_(chunk_size), stream{} {
  cur_.reserve(chunk_size + 16 * 1024);
  dest_.resize(out_size);
}

CompressedBuffer::~CompressedBuffer() {
  // flush the stream and free mem
  (void)Result();
}

auto CompressedBuffer::maybe_compress() -> void {
  if (cur_.size() > chunk_size_) {
    compress(Z_NO_FLUSH);
    cur_.clear();
  }
}

// compress the current chunk
auto CompressedBuffer::compress(int flush) -> void {
  stream.avail_in = cur_.size();
  stream.next_in =
      reinterpret_cast<z_const Bytef*>(const_cast<uint8_t*>(cur_.data()));

  static constexpr int kMinFree = 16 * 1024;
  static constexpr int kChunk = 32 * 1024;
  int err = Z_OK;
  while (stream.avail_in > 0 && err == Z_OK) {
    auto avail_out = dest_.size() - dest_index_;
    if (avail_out < kMinFree) {
      dest_.resize(dest_.size() + kChunk);
      avail_out += kChunk;
    }

    stream.next_out = reinterpret_cast<Bytef*>(dest_.data() + dest_index_);
    stream.avail_out = static_cast<uInt>(avail_out);

    err = deflate(&stream, flush);
    dest_index_ = stream.total_out;
  }
  assert(stream.avail_in == 0);
}

auto CompressedBuffer::Result() -> CompressedResult {
  if (init_) {
    init_ = false;
    compress(Z_FINISH);
    deflateEnd(&stream);
  }
  return CompressedResult{dest_.data(), stream.total_out};
}

auto CompressedBuffer::Init() -> void {
  cur_.clear();
  init_ = true;
  dest_index_ = 0;

  stream.zalloc = static_cast<alloc_func>(nullptr);
  stream.zfree = static_cast<free_func>(nullptr);
  stream.opaque = static_cast<voidpf>(nullptr);

  auto err = deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, kWindowBits,
                          kMemLevel, Z_DEFAULT_STRATEGY);
  if (err != Z_OK) {
    throw std::runtime_error(fmt::format("Unable to init zlib: {}", err));
  }
}

}  // namespace spectator
