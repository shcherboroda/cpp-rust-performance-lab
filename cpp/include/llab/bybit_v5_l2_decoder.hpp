#pragma once

#include "llab/bybit_l2_order_book.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cctype>
#include <cstdint>
#include <charconv>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace llab::bybit_v5 {

enum class MessageKind { Control, Snapshot, Delta };
struct Message {
    MessageKind kind = MessageKind::Control;
    std::uint64_t update_id = 0;
    std::uint64_t sequence = 0;
    std::uint64_t exchange_timestamp_ms = 0;
    std::string topic;
    std::vector<bybit_l2::Level> bids;
    std::vector<bybit_l2::Level> asks;
    std::vector<bybit_l2::Change> changes;
};

inline std::uint64_t parse_fixed_1e8(std::string_view text) {
    if (text.empty() || text.front() == '-') throw std::invalid_argument("expected unsigned decimal");
    std::uint64_t whole = 0;
    std::size_t at = 0;
    std::size_t whole_digits = 0;
    while (at < text.size() && text[at] != '.') {
        if (!std::isdigit(static_cast<unsigned char>(text[at])) || whole > (std::numeric_limits<std::uint64_t>::max() / 10))
            throw std::invalid_argument("invalid decimal whole part");
        whole = whole * 10 + static_cast<std::uint64_t>(text[at++] - '0');
        ++whole_digits;
    }
    if (whole_digits == 0) throw std::invalid_argument("missing decimal whole part");
    std::uint64_t fractional = 0;
    std::size_t digits = 0;
    if (at < text.size()) ++at;
    while (at < text.size()) {
        if (!std::isdigit(static_cast<unsigned char>(text[at])) || digits == 8)
            throw std::invalid_argument("invalid decimal fractional part");
        fractional = fractional * 10 + static_cast<std::uint64_t>(text[at++] - '0');
        ++digits;
    }
    while (digits++ < 8) fractional *= 10;
    if (whole > (std::numeric_limits<std::uint64_t>::max() - fractional) / 100000000ULL)
        throw std::invalid_argument("decimal overflow");
    return whole * 100000000ULL + fractional;
}

inline std::uint64_t parse_u64(const std::string& text) {
    if (text.empty()) throw std::invalid_argument("invalid unsigned integer");
    std::uint64_t result = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (error != std::errc{} || end != text.data() + text.size()) throw std::invalid_argument("invalid unsigned integer");
    return result;
}

inline void decode_levels(const boost::property_tree::ptree& input, const bybit_l2::Side side,
                          std::vector<bybit_l2::Level>& levels, std::vector<bybit_l2::Change>& changes,
                          const bool snapshot) {
    for (const auto& row : input) {
        auto field = row.second.begin();
        if (field == row.second.end()) throw std::invalid_argument("level has no price");
        const auto price = parse_fixed_1e8(field++->second.get_value<std::string>());
        if (field == row.second.end()) throw std::invalid_argument("level has no size");
        const auto quantity = parse_fixed_1e8(field++->second.get_value<std::string>());
        if (field != row.second.end()) throw std::invalid_argument("level has unexpected fields");
        if (price == 0) throw std::invalid_argument("level contains zero price");
        if (snapshot) {
            if (quantity == 0) throw std::invalid_argument("snapshot contains zero size");
            levels.push_back({price, quantity});
        } else {
            changes.push_back({side, price, quantity});
        }
    }
}

// This adapter intentionally uses Boost.PropertyTree as a correctness-first
// parser. It is outside book-application benchmarks and can later be replaced
// by a SIMD parser without changing Message or book semantics.
inline std::optional<Message> decode(std::string_view payload) {
    boost::property_tree::ptree root;
    std::istringstream stream{std::string(payload)};
    boost::property_tree::read_json(stream, root);
    const auto type = root.get_optional<std::string>("type");
    if (!type) return Message{};  // subscribe/ping acknowledgements and other controls.
    if (*type != "snapshot" && *type != "delta") throw std::invalid_argument("unknown Bybit book message type");
    const auto& data = root.get_child("data");
    Message result;
    result.kind = *type == "snapshot" ? MessageKind::Snapshot : MessageKind::Delta;
    result.update_id = parse_u64(data.get<std::string>("u"));
    if (result.update_id == 0) throw std::invalid_argument("zero update id");
    result.sequence = parse_u64(data.get<std::string>("seq", "0"));
    result.exchange_timestamp_ms = parse_u64(root.get<std::string>("cts", "0"));
    result.topic = root.get<std::string>("topic");
    const bool snapshot = result.kind == MessageKind::Snapshot;
    decode_levels(data.get_child("b"), bybit_l2::Side::Bid, result.bids, result.changes, snapshot);
    decode_levels(data.get_child("a"), bybit_l2::Side::Ask, result.asks, result.changes, snapshot);
    return result;
}

inline bool apply(const Message& message, bybit_l2::OrderBook& book) {
    if (message.kind == MessageKind::Control) return true;
    return message.kind == MessageKind::Snapshot
               ? book.apply_snapshot(message.update_id, message.bids, message.asks)
               : book.apply_delta(message.update_id, message.changes);
}

}  // namespace llab::bybit_v5
