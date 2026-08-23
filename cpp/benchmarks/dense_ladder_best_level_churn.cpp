#ifdef LLAB_BITMAP_V4
#include "llab/bitmap_ladder_order_book.hpp"
#else
#include "llab/dense_ladder_order_book.hpp"
#endif

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
using llab::parity_order_book::Side;
#ifdef LLAB_BITMAP_V4
using llab::bitmap_ladder_order_book::OrderBook;
constexpr const char *kBenchmarkName = "bitmap_ladder_best_level_churn_v4";
#else
using llab::dense_ladder_order_book::OrderBook;
constexpr const char *kBenchmarkName = "dense_ladder_best_level_churn_v3";
#endif

constexpr std::size_t kOrderCount = 32'768;
constexpr std::size_t kWarmupSamples = 10;
constexpr std::size_t kMeasuredSamples = 200;

std::vector<Event> make_best_level_churn_trace() {
    std::vector<Event> events;
    events.reserve(kOrderCount * 2);
    for (std::size_t index = 0; index < kOrderCount; ++index) {
        const auto side = index % 2 == 0 ? Side::Bid : Side::Ask;
        const auto price = side == Side::Bid ? 100'128ULL : 99'873ULL;
        const auto quantity = 1ULL + index % 97;
        events.push_back({EventType::Add, index + 1, 0, side, price, quantity});
        events.push_back({EventType::Execute, index + 1, 0, side, price, quantity});
    }
    return events;
}

std::uint64_t bbo_digest(const OrderBook &book) {
    const auto bid = book.best_bid();
    const auto ask = book.best_ask();
    std::uint64_t digest = 0x9E3779B97F4A7C15ULL;
    if (bid)
        digest ^= bid->price_ticks ^ (bid->quantity << 1U);
    if (ask)
        digest ^= (ask->price_ticks << 7U) ^ (ask->quantity << 3U);
    return digest;
}

std::uint64_t run_sample(const std::vector<Event> &events, std::uint64_t &digest) {
    OrderBook book(kOrderCount, 99'873, 256);
    const auto start = std::chrono::steady_clock::now();
    for (const Event &event : events) {
        book.apply(event);
        digest ^= bbo_digest(book) + 0x9E3779B97F4A7C15ULL + (digest << 6U) + (digest >> 2U);
    }
    const auto end = std::chrono::steady_clock::now();
    if (book.live_order_count() != 0 || book.best_bid() || book.best_ask())
        throw std::runtime_error("book not empty");
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

std::size_t rank(std::size_t n, long double p) {
    return static_cast<std::size_t>(std::ceil(n * p)) - 1;
}
} // namespace

int main(int argc, char **argv) {
    std::string raw_path;
    if (argc == 3 && std::string_view(argv[1]) == "--raw")
        raw_path = argv[2];
    else if (argc != 1) {
        std::cerr << "usage: dense_ladder_best_level_churn [--raw PATH]\n";
        return 2;
    }
    const auto events = make_best_level_churn_trace();
    const OrderBook layout_book(kOrderCount, 99'873, 256);
    std::uint64_t digest = 0;
    for (std::size_t i = 0; i < kWarmupSamples; ++i)
        (void)run_sample(events, digest);
    std::vector<std::uint64_t> samples;
    samples.reserve(kMeasuredSamples);
    for (std::size_t i = 0; i < kMeasuredSamples; ++i)
        samples.push_back(run_sample(events, digest));
    if (!raw_path.empty()) {
        std::ofstream raw(raw_path);
        if (!raw)
            throw std::runtime_error("cannot write raw");
        raw << "sample_index,duration_ns\n";
        for (std::size_t i = 0; i < samples.size(); ++i)
            raw << i << ',' << samples[i] << '\n';
    }
    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    long double total = 0;
    for (auto sample : samples)
        total += sample;
    const auto p50 = sorted[rank(sorted.size(), .50L)];
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "format: llab_benchmark_v1\nbenchmark: " << kBenchmarkName << "\nlanguage: cpp\n";
    std::cout << "event_size_bytes: " << sizeof(Event)
              << "\norder_book_size_bytes: " << sizeof(OrderBook)
              << "\norder_index_capacity: " << layout_book.order_index_capacity() << '\n';
    std::cout << "orders_per_sample: " << kOrderCount << "\nevents_per_sample: " << events.size()
              << "\nwarmup_samples: " << kWarmupSamples
              << "\nmeasured_samples: " << kMeasuredSamples << '\n';
    std::cout << "min_ns: " << sorted.front() << "\np50_ns: " << p50
              << "\np90_ns: " << sorted[rank(sorted.size(), .90L)]
              << "\np95_ns: " << sorted[rank(sorted.size(), .95L)]
              << "\np99_ns: " << sorted[rank(sorted.size(), .99L)]
              << "\np999_ns: " << sorted[rank(sorted.size(), .999L)]
              << "\nmax_ns: " << sorted.back() << "\nmean_ns: " << total / samples.size()
              << "\np50_ns_per_event: " << static_cast<long double>(p50) / events.size()
              << "\np50_events_per_s: "
              << static_cast<long double>(events.size()) * 1'000'000'000.0L / p50
              << "\nresult_digest: " << digest << '\n';
}
