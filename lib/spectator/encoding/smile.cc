#include "smile.h"
namespace spectator
{

inline auto zigzagEncode(size_t input) -> size_t { return input << 1U; }

// Small ints are 4-bit (-16 to +15) integer constants
static constexpr uint8_t kTokenPrefixSmallInt = 0xC0;

// Type prefixes

static constexpr uint8_t kTokenPrefixInteger = 0x24;
static constexpr uint8_t kTokenPrefixFP = 0x28;

// Numeric subtype
static constexpr uint8_t kTokenInt32 = 0x0;
// static constexpr uint8_t kTokenInt64 = 0x1;
// static constexpr uint8_t kTokenFloat32 = 0x0;
static constexpr uint8_t kTokenFloat64 = 0x1;

static constexpr uint8_t kByteInt32 = kTokenPrefixInteger + kTokenInt32;
// static constexpr uint8_t kByteInt64 =  kTokenPrefixInteger + kTokenInt64;
static constexpr uint8_t kByteFloat64 = kTokenPrefixFP + kTokenFloat64;

void SmilePayload::Append(int n)
{
	assert(n >= 0);
	Append(static_cast<size_t>(n));
}

void SmilePayload::Append(size_t n)
{
	auto i = zigzagEncode(n);
	if (i <= 0x3F)
	{
		if (i <= 0x1F)
		{  // tiny
			buffer_.Append(static_cast<uint8_t>(kTokenPrefixSmallInt + i));
			return;
		}
		// not tiny, just small
		buffer_.Append(kByteInt32, static_cast<uint8_t>(0x80 + i));
		return;
	}
	auto b0 = static_cast<uint8_t>(0x80 + (i & 0x3FU));
	i >>= 6U;
	if (i <= 0x7F)
	{  // 13 bits is enough (== 3 bytes total encoding)
		buffer_.Append(kByteInt32, static_cast<uint8_t>(i), b0);
		return;
	}
	auto b1 = static_cast<uint8_t>(i & 0x7FU);
	i >>= 7U;
	if (i <= 0x7F)
	{
		buffer_.Append(kByteInt32, static_cast<uint8_t>(i), b1, b0);
		return;
	}
	auto b2 = static_cast<uint8_t>(i & 0x7FU);
	i >>= 7U;
	if (i <= 0x7F)
	{
		buffer_.Append(kTokenInt32, static_cast<uint8_t>(i), b2, b1, b0);
		return;
	}
	// no, need all 5 bytes
	auto b3 = static_cast<uint8_t>(i & 0x7FU);
	buffer_.Append(kByteInt32, static_cast<uint8_t>(i >> 7U), b3, b2, b1, b0);
}

union LongDouble
{
	double d;
	uint64_t l;
};

void SmilePayload::Append(double value)
{
	buffer_.Append(kByteFloat64);
	LongDouble ld{value};
	auto l = ld.l;

	// Handle first 29 bits (single bit first, then 4 x 7 bits)
	auto hi5 = static_cast<uint32_t>(l >> 35U);
	auto b4 = static_cast<uint8_t>(hi5 & 0x7FU);
	hi5 >>= 7U;
	auto b3 = static_cast<uint8_t>(hi5 & 0x7FU);
	hi5 >>= 7U;
	auto b2 = static_cast<uint8_t>(hi5 & 0x7FU);
	hi5 >>= 7U;
	auto b1 = static_cast<uint8_t>(hi5 & 0x7FU);
	hi5 >>= 7U;
	auto b0 = static_cast<uint8_t>(hi5);
	buffer_.Append(b0, b1, b2, b3, b4);

	// Then split byte (one that crosses lo/hi int boundary), 7 bits
	{
		auto mid = static_cast<uint32_t>(l >> 28U);
		buffer_.Append(static_cast<uint8_t>(mid & 0x7FU));
	}

	// and then last 4 bytes (28 bits)
	auto lo4 = static_cast<uint32_t>(l);
	b3 = static_cast<uint8_t>(lo4 & 0x7FU);
	lo4 >>= 7U;
	b2 = static_cast<uint8_t>(lo4 & 0x7FU);
	lo4 >>= 7U;
	b1 = static_cast<uint8_t>(lo4 & 0x7FU);
	lo4 >>= 7U;
	b0 = static_cast<uint8_t>(lo4 & 0x7FU);
	buffer_.Append(b0, b1, b2, b3);
}

void SmilePayload::Append(std::string_view s)
{
	static constexpr uint8_t kLiteralEmptyString = 0x20U;
	static constexpr uint8_t kMaxShortValueStringBytes = 64;
	static constexpr uint8_t kTinyAscii = 0x40;
	static constexpr uint8_t kLongString = 0xE0;
	static constexpr uint8_t kEndOfString = 0xFC;

	// we only support ascii
	if (s.empty())
	{
		buffer_.Append(kLiteralEmptyString);
		return;
	}

	auto len = s.length();
	if (len <= kMaxShortValueStringBytes)
	{
		buffer_.Append(static_cast<uint8_t>(kTinyAscii - 1 + len));
		buffer_.Append(s);
	}
	else
	{
		buffer_.Append(kLongString);
		buffer_.Append(s);
		buffer_.Append(kEndOfString);
	}
}
}  // namespace spectator
