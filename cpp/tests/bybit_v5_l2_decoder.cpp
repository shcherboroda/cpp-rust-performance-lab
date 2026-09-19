#include "llab/bybit_v5_l2_decoder.hpp"

#include <stdexcept>

int main() {
    using namespace llab;
    const auto snapshot = bybit_v5::decode(R"({"topic":"orderbook.50.BTCUSDT","type":"snapshot","cts":123,"data":{"s":"BTCUSDT","b":[["100.25","2"],["100.00","1.5"]],"a":[["100.50","3"]],"u":10,"seq":20}})");
    if (!snapshot || snapshot->kind != bybit_v5::MessageKind::Snapshot || snapshot->update_id != 10 ||
        snapshot->bids[0] != bybit_l2::Level{10'025'000'000ULL, 200'000'000ULL})
        throw std::runtime_error("snapshot decode failed");
    bybit_l2::OrderBook book(50);
    if (!bybit_v5::apply(*snapshot, book)) throw std::runtime_error("snapshot apply failed");
    const auto delta = bybit_v5::decode(R"({"topic":"orderbook.50.BTCUSDT","type":"delta","cts":124,"data":{"b":[["100.25","0"]],"a":[["100.40","4"]],"u":11,"seq":21}})");
    if (!delta || !bybit_v5::apply(*delta, book) || book.best_bid() != bybit_l2::Level{10'000'000'000ULL, 150'000'000ULL} ||
        book.best_ask() != bybit_l2::Level{10'040'000'000ULL, 400'000'000ULL})
        throw std::runtime_error("delta decode/apply failed");
    try {
        static_cast<void>(bybit_v5::parse_fixed_1e8("1.000000001"));
        throw std::runtime_error("overprecision accepted");
    } catch (const std::invalid_argument&) {
    }
    try {
        static_cast<void>(bybit_v5::parse_fixed_1e8(".1"));
        throw std::runtime_error("missing whole part accepted");
    } catch (const std::invalid_argument&) {
    }
    return 0;
}
