#include "llab/bybit_l2_order_book.hpp"
#include "llab/bybit_v5_l2_decoder.hpp"
#include "llab/raw_frame_capture.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 3) throw std::runtime_error("usage: bybit_l2_state_export CAPTURE.llfr OUTPUT.csv");
    llab::RawFrameCaptureReader reader(argv[1]);
    const auto first = reader.metadata().topic.find('.'), second = reader.metadata().topic.find('.', first + 1);
    llab::bybit_l2::OrderBook book(std::stoull(reader.metadata().topic.substr(first + 1, second - first - 1)));
    const std::string output = argv[2], partial = output + ".partial";
    if (std::filesystem::exists(output)) throw std::runtime_error("refusing to overwrite state export");
    std::ofstream out(partial);
    if (!out) throw std::runtime_error("cannot open state export");
    out << "cts_ms,update_id,best_bid_1e8,best_ask_1e8,state_digest,top10_bids,top10_asks\n";
    std::size_t rows = 0;
    while (const auto frame = reader.next()) {
        if (frame->direction != llab::FrameDirection::Inbound || frame->kind != llab::FrameKind::Text) continue;
        const auto message = llab::bybit_v5::decode({reinterpret_cast<const char*>(frame->payload.data()), frame->payload.size()});
        if (!message || message->kind == llab::bybit_v5::MessageKind::Control) continue;
        if (message->topic != reader.metadata().topic || !llab::bybit_v5::apply(*message, book)) throw std::runtime_error("invalid state transition");
        const auto bid = book.best_bid(), ask = book.best_ask(); if (!bid || !ask) continue;
        auto levels = [&](llab::bybit_l2::Side side) { std::string value; for (const auto& l : book.top_levels(side, 10)) { if (!value.empty()) value += ';'; value += std::to_string(l.price) + ':' + std::to_string(l.quantity); } return value; };
        out << message->exchange_timestamp_ms << ',' << message->update_id << ',' << bid->price << ',' << ask->price << ',' << book.state_digest() << ',' << levels(llab::bybit_l2::Side::Bid) << ',' << levels(llab::bybit_l2::Side::Ask) << '\n';
        ++rows;
    }
    out.flush(); out.close();
    if (!out) throw std::runtime_error("cannot finalize state export");
    std::filesystem::rename(partial, output);
    std::ofstream manifest(output + ".meta");
    manifest << "schema=bybit_l2_state_export_v1\nsource=" << argv[1] << "\ntopic=" << reader.metadata().topic << "\nrows=" << rows << '\n';
    if (!manifest) throw std::runtime_error("cannot write state export manifest");
}
