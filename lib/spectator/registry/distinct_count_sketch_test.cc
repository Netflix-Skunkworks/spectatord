#include <gtest/gtest.h>
#include "distinct_count_sketch.h"
#include "test_utils.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "xxhash.h"

// Verifies the DistinctCountSketch hashing and register encoding against a committed set of
// cross-language test vectors generated from the spectator-java reference implementation.
// The same file lives in both repositories; if it is regenerated there it must be copied
// here. Any mismatch is a compatibility break that would prevent sketches published by
// different clients from merging correctly.
namespace
{
using namespace spectator;

constexpr int kRegisters = DistinctCountSketch::kRegisters;
constexpr const char* kVectorFile = "test-resources/distinct_count_sketch_test_vectors.txt";

enum class Type
{
	Long,
	Str,
	Bytes
};

struct Input
{
	Type type;
	int64_t long_value;
	std::string bytes;  // UTF-8 bytes for Str, raw bytes for Bytes
};

// Compute the xxHash64 (seed 0) the same way the sketch does for each input type. This is an
// independent check that the C++ hashing matches the hash column produced by spectator-java.
auto hash_of(const Input& in) -> uint64_t
{
	if (in.type == Type::Long)
	{
		std::array<uint8_t, 8> b{};
		auto v = static_cast<uint64_t>(in.long_value);
		for (int i = 0; i < 8; ++i)
		{
			b[i] = static_cast<uint8_t>(v >> (8 * i));
		}
		return XXH64(b.data(), b.size(), 0);
	}
	return XXH64(in.bytes.data(), in.bytes.size(), 0);
}

void record_into(DistinctCountSketch* sketch, const Input& in)
{
	switch (in.type)
	{
		case Type::Long:
			sketch->Record(in.long_value);
			break;
		case Type::Str:
			sketch->Record(std::string_view{in.bytes});
			break;
		case Type::Bytes:
			sketch->Record(in.bytes.data(), in.bytes.size());
			break;
	}
}

auto unescape(const std::string& s) -> std::string
{
	std::string out;
	for (size_t i = 0; i < s.size(); ++i)
	{
		char c = s[i];
		if (c == '\\' && i + 1 < s.size())
		{
			char n = s[++i];
			switch (n)
			{
				case '\\': out += '\\'; break;
				case 't': out += '\t'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				default: out += n; break;
			}
		}
		else
		{
			out += c;
		}
	}
	return out;
}

auto from_hex(const std::string& hex) -> std::string
{
	auto nibble = [](char c) -> int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		throw std::invalid_argument("invalid hex digit");
	};
	std::string out;
	out.reserve(hex.size() / 2);
	for (size_t i = 0; i + 1 < hex.size(); i += 2)
	{
		out += static_cast<char>((nibble(hex[i]) << 4) | nibble(hex[i + 1]));
	}
	return out;
}

auto decode_input(const std::string& type, const std::string& value) -> Input
{
	if (type == "long") return Input{Type::Long, std::stoll(value), {}};
	if (type == "str") return Input{Type::Str, 0, unescape(value)};
	if (type == "bytes") return Input{Type::Bytes, 0, from_hex(value)};
	throw std::invalid_argument("unknown type: " + type);
}

// Split keeping empty fields (an empty string/bytes value is an empty field).
auto split(const std::string& s, char delim) -> std::vector<std::string>
{
	std::vector<std::string> out;
	size_t start = 0;
	while (true)
	{
		auto pos = s.find(delim, start);
		if (pos == std::string::npos)
		{
			out.push_back(s.substr(start));
			break;
		}
		out.push_back(s.substr(start, pos - start));
		start = pos + 1;
	}
	return out;
}

auto read_lines(const char* path) -> std::vector<std::string>
{
	std::ifstream in{path};
	EXPECT_TRUE(in.is_open()) << "missing vector file: " << path;
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(in, line))
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();  // tolerate CRLF
		}
		lines.push_back(line);
	}
	return lines;
}

// Read the current value (max rho) of each register, mapping the unset NaN to 0.
auto read_registers(Registry* r, const Id& id) -> std::array<int, kRegisters>
{
	std::array<int, kRegisters> regs{};
	for (int i = 0; i < kRegisters; ++i)
	{
		char buf[4];
		std::snprintf(buf, sizeof(buf), "R%02X", i);
		auto reg_id = id.WithTags(refs().statistic(), refs().distinct(), refs().distinct(), intern_str(buf));
		double v = r->GetMaxGauge(std::move(reg_id))->Get();
		regs[i] = std::isnan(v) ? 0 : static_cast<int>(v);
	}
	return regs;
}

// Parse the [encoding] section into inputs and their expected hash/index/rho.
struct EncodingVector
{
	Input input;
	uint64_t hash;
	int index;
	int rho;
};

auto parse_encoding(const std::vector<std::string>& lines) -> std::vector<EncodingVector>
{
	std::vector<EncodingVector> result;
	bool in_section = false;
	for (const auto& line : lines)
	{
		if (line.empty() || line[0] == '#') continue;
		if (line[0] == '[')
		{
			in_section = (line == "[encoding]");
			continue;
		}
		if (!in_section) continue;
		auto f = split(line, '\t');
		if (f.size() != 5U)
		{
			ADD_FAILURE() << "malformed encoding line: " << line;
			continue;
		}
		result.push_back(EncodingVector{decode_input(f[0], f[1]), std::stoull(f[2], nullptr, 16), std::stoi(f[3]),
		                                 std::stoi(f[4])});
	}
	return result;
}

auto inputs_for_spec(const std::string& spec, const std::vector<EncodingVector>& encoding) -> std::vector<Input>
{
	std::vector<Input> inputs;
	if (spec == "encoding-inputs")
	{
		for (const auto& ev : encoding) inputs.push_back(ev.input);
		return inputs;
	}
	auto parts = split(spec, ':');
	if (parts.size() == 3 && parts[0] == "longs")
	{
		int64_t begin = std::stoll(parts[1]);
		int64_t end = std::stoll(parts[2]);
		for (int64_t i = begin; i < end; ++i) inputs.push_back(Input{Type::Long, i, {}});
		return inputs;
	}
	throw std::invalid_argument("unknown sketch spec: " + spec);
}

TEST(DistinctCountSketchVector, EncodingVectors)
{
	auto lines = read_lines(kVectorFile);
	auto encoding = parse_encoding(lines);
	ASSERT_FALSE(encoding.empty());

	for (const auto& ev : encoding)
	{
		// 1. The hash matches spectator-java (xxHash64 over the same bytes).
		EXPECT_EQ(hash_of(ev.input), ev.hash);
		// 2. The register derivation matches.
		EXPECT_EQ(DistinctCountSketch::register_index(ev.hash), ev.index);
		EXPECT_EQ(DistinctCountSketch::register_rho(ev.hash), ev.rho);

		// 3. Recording the value through the public API populates exactly the expected register.
		Registry r{GetConfiguration(), spectatord::Logger()};
		DistinctCountSketch sketch{&r, Id::Of("test")};
		record_into(&sketch, ev.input);
		auto regs = read_registers(&r, Id::Of("test"));
		int populated = 0;
		for (int i = 0; i < kRegisters; ++i)
		{
			if (regs[i] != 0)
			{
				++populated;
				EXPECT_EQ(i, ev.index);
				EXPECT_EQ(regs[i], ev.rho);
			}
		}
		EXPECT_EQ(populated, 1);
	}
}

TEST(DistinctCountSketchVector, SketchVectors)
{
	auto lines = read_lines(kVectorFile);
	auto encoding = parse_encoding(lines);

	bool in_section = false;
	int checked = 0;
	for (const auto& line : lines)
	{
		if (line.empty() || line[0] == '#') continue;
		if (line[0] == '[')
		{
			in_section = (line == "[sketch]");
			continue;
		}
		if (!in_section) continue;

		auto f = split(line, '\t');
		ASSERT_EQ(f.size(), 2U) << "malformed sketch line: " << line;
		auto values = split(f[1], ',');
		ASSERT_EQ(values.size(), static_cast<size_t>(kRegisters));
		std::array<int, kRegisters> expected{};
		for (int i = 0; i < kRegisters; ++i) expected[i] = std::stoi(values[i]);

		Registry r{GetConfiguration(), spectatord::Logger()};
		DistinctCountSketch sketch{&r, Id::Of("test")};
		for (const auto& in : inputs_for_spec(f[0], encoding)) record_into(&sketch, in);

		EXPECT_EQ(read_registers(&r, Id::Of("test")), expected) << "sketch mismatch: " << f[0];
		++checked;
	}
	EXPECT_GT(checked, 0);
}

}  // namespace
