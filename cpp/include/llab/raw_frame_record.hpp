#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace llab {
enum class FrameDirection : std::uint8_t {
    Inbound = 0,
    Outbound = 1,
};

enum class FrameKind : std::uint8_t {
    Text = 0,
    Binary = 1,
    Control = 2,
    Lifecycle = 3,
};

struct RawFrameRecord {
    std::uint64_t capture_index;
    std::uint64_t monotonic_ns;
    std::uint64_t connection_id;
    std::int64_t utc_ns;
    FrameDirection direction;
    FrameKind kind;
    std::vector<std::uint8_t> payload;

    bool operator==(const RawFrameRecord&) const = default;
};

constexpr std::size_t raw_frame_header_size = 38;
constexpr std::size_t raw_frame_max_payload = 1U << 20;

inline std::uint32_t crc32c(const std::span<const std::uint8_t> bytes) {
    std::uint32_t value = ~0U;
    for (const auto byte : bytes) {
        value ^= byte;
        for (int bit = 0; bit != 8; ++bit) {
            value = (value >> 1) ^ (0x82F63B78U & (-(value & 1U)));
        }
    }
    return ~value;
}

template <class T>
inline void append_le(std::vector<std::uint8_t>& out, const T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<std::uint8_t>(static_cast<std::uint64_t>(value) >> (i * 8)));
    }
}

template <class T>
inline T read_le(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value |= static_cast<std::uint64_t>(bytes[offset + i]) << (i * 8);
    }
    return static_cast<T>(value);
}

inline void encode_raw_frame_into(const RawFrameRecord& record, std::vector<std::uint8_t>& out) {
    out.clear();
    out.reserve(raw_frame_header_size + record.payload.size() + 4);
    append_le(out, record.capture_index);
    append_le(out, record.monotonic_ns);
    append_le(out, record.utc_ns);
    append_le(out, record.connection_id);
    out.push_back(static_cast<std::uint8_t>(record.direction));
    out.push_back(static_cast<std::uint8_t>(record.kind));
    append_le(out, static_cast<std::uint32_t>(record.payload.size()));
    out.insert(out.end(), record.payload.begin(), record.payload.end());
    append_le(out, crc32c(out));
}

inline std::vector<std::uint8_t> encode_raw_frame(const RawFrameRecord& record) {
    std::vector<std::uint8_t> out;
    encode_raw_frame_into(record, out);
    return out;
}

inline std::optional<RawFrameRecord> decode_raw_frame(const std::span<const std::uint8_t> bytes) {
    if (bytes.size() < raw_frame_header_size + 4) {
        return std::nullopt;
    }

    const auto length = read_le<std::uint32_t>(bytes, 34);
    if (length > raw_frame_max_payload || bytes.size() != raw_frame_header_size + length + 4 ||
        crc32c(bytes.first(bytes.size() - 4)) != read_le<std::uint32_t>(bytes, bytes.size() - 4)) {
        return std::nullopt;
    }

    return RawFrameRecord{
        read_le<std::uint64_t>(bytes, 0),
        read_le<std::uint64_t>(bytes, 8),
        read_le<std::uint64_t>(bytes, 24),
        read_le<std::int64_t>(bytes, 16),
        static_cast<FrameDirection>(bytes[32]),
        static_cast<FrameKind>(bytes[33]),
        {bytes.begin() + raw_frame_header_size, bytes.begin() + raw_frame_header_size + length},
    };
}
}  // namespace llab
