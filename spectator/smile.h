#pragma once

#include "compressed_buffer.h"

namespace spectator
{

// this is not a general class
// this is only meant to serialize the array of measurements
// we send to the atlas-aggregator
class SmilePayload
{
   public:
	void Init()
	{
		buffer_.Init();
		write_header();
		write_start_array();
	}
	void Append(int n);
	void Append(size_t n);
	void Append(double value);
	void Append(std::string_view s);
	auto Result() -> CompressedResult
	{
		write_end_array();
		return buffer_.Result();
	}

   private:
	CompressedBuffer buffer_;

	void write_header()
	{
		static constexpr uint8_t kHeaderByte1 = ':';
		static constexpr uint8_t kHeaderByte2 = ')';
		static constexpr uint8_t kHeaderByte3 = '\n';
		// we do not use any features, like shared strings. Otherwise
		// the 4th byte would need to be modified based on flags
		static constexpr uint8_t kHeaderByte4 = 0;

		buffer_.Append(kHeaderByte1, kHeaderByte2, kHeaderByte3, kHeaderByte4);
	}

	void write_start_array()
	{
		static constexpr uint8_t kStartArray = 0xF8;
		buffer_.Append(kStartArray);
	}

	void write_end_array()
	{
		static constexpr uint8_t kEndArray = 0xF9;
		buffer_.Append(kEndArray);
	}
};
}  // namespace spectator