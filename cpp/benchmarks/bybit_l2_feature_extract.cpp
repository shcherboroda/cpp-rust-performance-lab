#include "llab/bybit_l2_order_book.hpp"
#include "llab/bybit_l2_labels.hpp"
#include "llab/bybit_v5_l2_decoder.hpp"
#include "llab/raw_frame_capture.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Observation {
    std::uint64_t timestamp_ms, bid, ask, bid_size, ask_size;
    std::array<std::uint64_t, 3> bid_depth, ask_depth;
};

std::size_t topic_depth(const std::string& topic) {
    const auto first = topic.find('.'), second = topic.find('.', first + 1);
    if (first == std::string::npos || second == std::string::npos) throw std::runtime_error("invalid capture topic");
    return std::stoull(topic.substr(first + 1, second - first - 1));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) throw std::runtime_error("usage: bybit_l2_feature_extract CAPTURE.llfr OUTPUT.csv [MOVE_THRESHOLD_1E8]");
    const auto move_threshold = argc == 4 ? std::stoull(argv[3]) : 10'000'000ULL;
    if (move_threshold == 0) throw std::runtime_error("move threshold must be positive");
    llab::RawFrameCaptureReader reader(argv[1]);
    llab::bybit_l2::OrderBook book(topic_depth(reader.metadata().topic));
    std::vector<Observation> observations;
    while (const auto record = reader.next()) {
        if (record->direction != llab::FrameDirection::Inbound || record->kind != llab::FrameKind::Text) continue;
        const auto message = llab::bybit_v5::decode({reinterpret_cast<const char*>(record->payload.data()), record->payload.size()});
        if (!message || message->kind == llab::bybit_v5::MessageKind::Control) continue;
        if (message->topic != reader.metadata().topic || !llab::bybit_v5::apply(*message, book))
            throw std::runtime_error("capture contains invalid Bybit book transition");
        const auto bid = book.best_bid(), ask = book.best_ask();
        if (!bid || !ask || message->exchange_timestamp_ms == 0) continue;
        if (bid->price >= ask->price) throw std::runtime_error("crossed or locked BBO in capture");
        if (!observations.empty() && message->exchange_timestamp_ms < observations.back().timestamp_ms)
            throw std::runtime_error("capture exchange timestamps are not monotonic");
        observations.push_back({message->exchange_timestamp_ms, bid->price, ask->price, bid->quantity, ask->quantity,
                                {book.depth_quantity(llab::bybit_l2::Side::Bid, 1), book.depth_quantity(llab::bybit_l2::Side::Bid, 5), book.depth_quantity(llab::bybit_l2::Side::Bid, 25)},
                                {book.depth_quantity(llab::bybit_l2::Side::Ask, 1), book.depth_quantity(llab::bybit_l2::Side::Ask, 5), book.depth_quantity(llab::bybit_l2::Side::Ask, 25)}});
    }
    const std::string output_path = argv[2];
    const std::string partial_path = output_path + ".partial";
    if (std::filesystem::exists(output_path)) throw std::runtime_error("refusing to overwrite dataset");
    std::ofstream out(partial_path);
    if (!out) throw std::runtime_error("cannot open output CSV");
    out << "cts_ms,mid_1e8,spread_1e8,microprice_1e8,imbalance_1,imbalance_5,imbalance_25,label_100ms,label_500ms,label_1000ms\n";
    const std::array<std::uint64_t, 3> horizons{100, 500, 1000};
    std::array<std::size_t, 3> future{};
    std::size_t emitted = 0;
    for (std::size_t i = 0; i < observations.size(); ++i) {
        for (std::size_t h = 0; h < horizons.size(); ++h) {
            future[h] = std::max(future[h], i + 1);
            while (future[h] < observations.size() && observations[future[h]].timestamp_ms < observations[i].timestamp_ms + horizons[h]) ++future[h];
        }
        if (future.back() == observations.size()) break;
        const auto& x = observations[i]; const auto mid = (x.bid + x.ask) / 2;
        const auto imbalance = [](std::uint64_t bid, std::uint64_t ask) { return (static_cast<double>(bid) - static_cast<double>(ask)) / static_cast<double>(bid + ask); };
        const auto label = [&](std::size_t h) {
            const auto future_mid = (observations[future[h]].bid + observations[future[h]].ask) / 2;
            return llab::bybit_l2_labels::direction_label(mid, future_mid, move_threshold);
        };
        const auto micro = static_cast<std::uint64_t>(
            (static_cast<long double>(x.ask) * x.bid_size + static_cast<long double>(x.bid) * x.ask_size) /
            static_cast<long double>(x.bid_size + x.ask_size));
        out << x.timestamp_ms << ',' << mid << ',' << x.ask - x.bid << ',' << micro << ','
            << imbalance(x.bid_depth[0], x.ask_depth[0]) << ',' << imbalance(x.bid_depth[1], x.ask_depth[1]) << ',' << imbalance(x.bid_depth[2], x.ask_depth[2]) << ','
            << label(0) << ',' << label(1) << ',' << label(2) << '\n';
        ++emitted;
    }
    out.flush();
    out.close();
    if (!out) throw std::runtime_error("cannot finalize dataset CSV");
    std::filesystem::rename(partial_path, output_path);
    std::ofstream manifest(output_path + ".meta");
    manifest << "schema=bybit_l2_features_labels_v1\nsource=" << argv[1] << "\ntopic=" << reader.metadata().topic
             << "\nmove_threshold_1e8=" << move_threshold << "\nrows=" << emitted << '\n';
    if (!manifest) throw std::runtime_error("cannot write dataset manifest");
}
