#include "llab/parity_order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using llab::parity_order_book::Event;
using llab::parity_order_book::EventType;
using llab::parity_order_book::OrderBook;
using llab::parity_order_book::Side;

constexpr std::size_t kOrderCount = 32'768;
constexpr std::size_t kWarmupSamples = 10;
constexpr std::size_t kMeasuredSamples = 200;

std::vector<Event> make_lifecycle_churn_trace() {
    std::vector<Event> events;
    events.reserve(kOrderCount * 2);
    for (std::size_t index = 0; index < kOrderCount; ++index) {
        const bool is_bid = index % 2 == 0;
        const std::uint64_t offset = index % 128;
        const auto side = is_bid ? Side::Bid : Side::Ask;
        const std::uint64_t price = is_bid ? 100'000 - offset : 100'001 + offset;
        const std::uint64_t quantity = 1 + (index % 97);
        events.push_back({EventType::Add, index + 1, 0, side, price, quantity});
    }
    for (std::size_t index = 0; index < kOrderCount; ++index) {
        const std::uint64_t quantity = 1 + (index % 97);
        events.push_back({EventType::Execute, index + 1, 0, Side::Bid, 0, quantity});
    }
    return events;
}

std::uint64_t run_sample(const std::vector<Event> &events) {
    OrderBook book(kOrderCount);
    const auto start = std::chrono::steady_clock::now();
    for (const Event &event : events) {
        book.apply(event);
    }
    const auto end = std::chrono::steady_clock::now();
    if (book.live_order_count() != 0 || book.best_bid().has_value() ||
        book.best_ask().has_value()) {
        throw std::runtime_error("lifecycle trace did not restore an empty order book");
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

std::size_t nearest_rank_index(std::size_t count, long double percentile) {
    return static_cast<std::size_t>(std::ceil(percentile * count)) - 1;
}

} // namespace

int main(int argc, char **argv) {
    std::string raw_path;
    if (argc == 3 && std::string_view(argv[1]) == "--raw") {
        raw_path = argv[2];
    } else if (argc != 1) {
        std::cerr << "usage: order_book_lifecycle [--raw PATH]\n";
        return 2;
    }

    const std::vector<Event> events = make_lifecycle_churn_trace();
    const OrderBook layout_book(kOrderCount);
    for (std::size_t sample = 0; sample < kWarmupSamples; ++sample) {
        (void)run_sample(events);
    }

    std::vector<std::uint64_t> samples;
    samples.reserve(kMeasuredSamples);
    for (std::size_t sample = 0; sample < kMeasuredSamples; ++sample) {
        samples.push_back(run_sample(events));
    }
    if (!raw_path.empty()) {
        std::ofstream raw(raw_path);
        if (!raw)
            throw std::runtime_error("cannot write raw sample file");
        raw << "sample_index,duration_ns\n";
        for (std::size_t index = 0; index < samples.size(); ++index)
            raw << index << ',' << samples[index] << '\n';
    }

    const auto sorted = [&] {
        auto copy = samples;
        std::sort(copy.begin(), copy.end());
        return copy;
    }();
    long double total = 0;
    for (const auto sample : samples)
        total += sample;
    const long double mean = total / samples.size();
    const long double p50 = sorted[nearest_rank_index(sorted.size(), 0.50L)];
    const long double events_per_second =
        static_cast<long double>(events.size()) * 1'000'000'000.0L / p50;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "format: llab_benchmark_v1\n";
    std::cout << "benchmark: parity_order_book_lifecycle_churn_v2\n";
    std::cout << "language: cpp\n";
    std::cout << "event_size_bytes: " << sizeof(Event) << '\n';
    std::cout << "order_book_size_bytes: " << sizeof(OrderBook) << '\n';
    std::cout << "order_index_capacity: " << layout_book.order_index_capacity() << '\n';
    std::cout << "orders_per_sample: " << kOrderCount << '\n';
    std::cout << "events_per_sample: " << events.size() << '\n';
    std::cout << "warmup_samples: " << kWarmupSamples << '\n';
    std::cout << "measured_samples: " << kMeasuredSamples << '\n';
    std::cout << "min_ns: " << sorted.front() << '\n';
    std::cout << "p50_ns: " << sorted[nearest_rank_index(sorted.size(), 0.50L)] << '\n';
    std::cout << "p90_ns: " << sorted[nearest_rank_index(sorted.size(), 0.90L)] << '\n';
    std::cout << "p95_ns: " << sorted[nearest_rank_index(sorted.size(), 0.95L)] << '\n';
    std::cout << "p99_ns: " << sorted[nearest_rank_index(sorted.size(), 0.99L)] << '\n';
    std::cout << "p999_ns: " << sorted[nearest_rank_index(sorted.size(), 0.999L)] << '\n';
    std::cout << "max_ns: " << sorted.back() << '\n';
    std::cout << "mean_ns: " << mean << '\n';
    std::cout << "p50_ns_per_event: " << p50 / events.size() << '\n';
    std::cout << "p50_events_per_s: " << events_per_second << '\n';
    return 0;
}
