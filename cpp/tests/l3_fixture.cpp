#include "llab/order_book.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using llab::order_book::Event;
using llab::order_book::EventType;
using llab::order_book::OrderBook;
using llab::order_book::Side;

Side parse_side(const std::string &text) {
    if (text == "B")
        return Side::Bid;
    if (text == "A")
        return Side::Ask;
    throw std::runtime_error("invalid side");
}

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream input(argv[1]);
    if (!input)
        return 2;

    OrderBook book;
    std::string line;
    bool header_seen = false;
    while (std::getline(input, line)) {
        if (line.empty() || line.starts_with('#'))
            continue;
        if (!header_seen) {
            require(line == "LLBT/1", "unexpected trace version");
            header_seen = true;
            continue;
        }
        if (line.starts_with("EXPECT ")) {
            std::uint64_t count, bid_price, bid_quantity, ask_price, ask_quantity, digest;
            std::istringstream expected(line.substr(7));
            expected >> count >> bid_price >> bid_quantity >> ask_price >> ask_quantity >> digest;
            require(book.live_order_count() == count, "live-order count mismatch");
            require(book.best_bid() ==
                        std::optional{llab::order_book::Level{bid_price, bid_quantity}},
                    "best bid mismatch");
            require(book.best_ask() ==
                        std::optional{llab::order_book::Level{ask_price, ask_quantity}},
                    "best ask mismatch");
            std::cout << book.state_digest() << '\n';
            if (digest != 0)
                require(book.state_digest() == digest, "state digest mismatch");
            return 0;
        }
        std::istringstream fields(line);
        char kind = '\0';
        fields >> kind;
        std::uint64_t id = 0, id2 = 0, price = 0, quantity = 0;
        std::string side;
        if (kind == 'A') {
            fields >> id >> side >> price >> quantity;
            book.apply({EventType::Add, id, 0, parse_side(side), price, quantity});
        } else if (kind == 'C' || kind == 'E') {
            fields >> id >> quantity;
            book.apply({kind == 'C' ? EventType::Cancel : EventType::Execute, id, 0, Side::Bid, 0,
                        quantity});
        } else if (kind == 'D' || kind == 'X') {
            fields >> id;
            book.apply({kind == 'D' ? EventType::Delete : EventType::OrderDelete, id});
        } else if (kind == 'R') {
            fields >> id >> id2 >> side >> price >> quantity;
            book.apply({EventType::Replace, id, id2, parse_side(side), price, quantity});
        } else if (kind == 'U') {
            fields >> id >> side >> price >> quantity;
            book.apply({EventType::OrderUpsert, id, 0, parse_side(side), price, quantity});
        } else {
            throw std::runtime_error("invalid trace record");
        }
    }
    return 2;
}
