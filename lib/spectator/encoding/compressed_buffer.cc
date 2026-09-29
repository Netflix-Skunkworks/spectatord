#include "compressed_buffer.h"
#include <fmt/format.h>

#ifndef z_const
#define z_const
#endif

namespace spectator
{

CompressedBuffer::CompressedBuffer(size_t chunk_size_input, size_t out_size, size_t chunk_size_output)
    : chunk_size_input_(chunk_size_input), chunk_size_output_(chunk_size_output), stream{}
{
	cur_.reserve(chunk_size_input + 16 * 1024);
	dest_.resize(out_size);
}

CompressedBuffer::~CompressedBuffer()
{
	// flush the stream and free mem
	(void)Result();
}

auto CompressedBuffer::maybe_compress() -> void
{
	if (cur_.size() > chunk_size_input_)
	{
		compress(Z_NO_FLUSH);
		cur_.clear();
	}
}

auto CompressedBuffer::deflate_chunk(size_t chunk_size, int flush) -> int
{
	auto avail_out = dest_.size() - dest_index_;
	if (avail_out < chunk_size / 2)
	{
		dest_.resize(dest_.size() + chunk_size);
		avail_out += chunk_size;
	}

	stream.next_out = reinterpret_cast<Bytef*>(dest_.data() + dest_index_);
	stream.avail_out = static_cast<uInt>(avail_out);

	int err = deflate(&stream, flush);
	dest_index_ = stream.total_out;
	return err;
}

// compress the current chunk
auto CompressedBuffer::compress(int flush) -> void
{
	stream.avail_in = cur_.size();
	stream.next_in = reinterpret_cast<z_const Bytef*>(const_cast<uint8_t*>(cur_.data()));

	int err = Z_OK;
	while (stream.avail_in > 0 && err == Z_OK)
	{
		err = deflate_chunk(chunk_size_output_, flush);
	}
	assert(stream.avail_in == 0);

	// If finishing, then we need to process until Z_STREAM_END. Since errors
	// are otherwise being ignored, anything that is not Z_OK will be assumed
	// to be the end of the stream.
	while (flush == Z_FINISH && err == Z_OK)
	{
		err = deflate_chunk(chunk_size_output_, flush);
	}
}

auto CompressedBuffer::Result() -> CompressedResult
{
	if (init_)
	{
		init_ = false;
		compress(Z_FINISH);
		deflateEnd(&stream);
	}
	return CompressedResult{dest_.data(), stream.total_out};
}

auto CompressedBuffer::Init() -> void
{
	cur_.clear();
	init_ = true;
	dest_index_ = 0;

	stream.zalloc = static_cast<alloc_func>(nullptr);
	stream.zfree = static_cast<free_func>(nullptr);
	stream.opaque = static_cast<voidpf>(nullptr);

	auto err = deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, kWindowBits, kMemLevel, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
	{
		throw std::runtime_error(fmt::format("Unable to init zlib: {}", err));
	}
}

}  // namespace spectator
