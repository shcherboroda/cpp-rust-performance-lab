#if defined(LLAB_NATIVE_V1)
#include "llab/order_book.hpp"
#elif defined(LLAB_PARITY_V2)
#include "llab/parity_order_book.hpp"
#elif defined(LLAB_DENSE_V3)
#include "llab/dense_ladder_order_book.hpp"
#elif defined(LLAB_BITMAP_V4)
#include "llab/bitmap_ladder_order_book.hpp"
#elif defined(LLAB_BITMAP_V5)
#include "llab/bitmap_backshift_order_book.hpp"
#elif defined(LLAB_BITMAP_V6)
#include "llab/bitmap_packed_order_book.hpp"
#else
#error "select one order-book version"
#endif

#if defined(LLAB_METRICS_DISABLED) || defined(LLAB_METRICS_CAPTURE) ||                             \
    defined(LLAB_METRICS_SAMPLED)
#include "llab/metrics.hpp"
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

#if defined(LLAB_NATIVE_V1)
using namespace llab::order_book;
constexpr const char *kVersion = "native_v1";
#elif defined(LLAB_PARITY_V2)
using namespace llab::parity_order_book;
constexpr const char *kVersion = "parity_v2";
#elif defined(LLAB_DENSE_V3)
using namespace llab::dense_ladder_order_book;
constexpr const char *kVersion = "dense_ladder_v3";
#elif defined(LLAB_BITMAP_V4)
using namespace llab::bitmap_ladder_order_book;
constexpr const char *kVersion = "bitmap_ladder_v4";
#elif defined(LLAB_BITMAP_V5)
using namespace llab::bitmap_backshift_order_book;
constexpr const char *kVersion = "bitmap_backshift_v5";
#else
using namespace llab::bitmap_packed_order_book;
constexpr const char *kVersion = "bitmap_packed_v6";
#endif

constexpr std::size_t kCycles = 8'192;
constexpr std::size_t kMaximumLiveOrders = 32'768;
constexpr std::size_t kWarmupSamples = 10;
constexpr std::size_t kMeasuredSamples = 200;

OrderBook make_book() {
#if defined(LLAB_DENSE_V3) || defined(LLAB_BITMAP_V4) || defined(LLAB_BITMAP_V5) ||                \
    defined(LLAB_BITMAP_V6)
    return OrderBook(kMaximumLiveOrders, 99'873, 256);
#else
    return OrderBook(kMaximumLiveOrders);
#endif
}

std::vector<Event> make_trace() {
    std::vector<Event> events;
    events.reserve(kCycles * 7);
    for (std::size_t cycle = 0; cycle < kCycles; ++cycle) {
        const OrderId a = cycle * 3 + 1;
        const OrderId b = cycle * 3 + 2;
        const OrderId c = cycle * 3 + 3;
        const Price offset = cycle % 128;
        const Side a_side = cycle % 2 == 0 ? Side::Bid : Side::Ask;
        const Side b_side = a_side == Side::Bid ? Side::Ask : Side::Bid;
        const Price a_price = a_side == Side::Bid ? 100'000 - offset : 100'001 + offset;
        const Price b_price = b_side == Side::Bid ? 100'000 - offset : 100'001 + offset;

        events.push_back({EventType::Add, a, 0, a_side, a_price, 10});
        events.push_back({EventType::Add, b, 0, b_side, b_price, 20});
        events.push_back({EventType::Cancel, a, 0, a_side, 0, 3});
        events.push_back({EventType::OrderUpsert, b, 0, b_side, b_price, 15});
        events.push_back({EventType::Replace, a, c, a_side, a_price, 7});
        events.push_back({EventType::Execute, c, 0, a_side, 0, 7});
        events.push_back({cycle % 2 == 0 ? EventType::Delete : EventType::OrderDelete, b});
    }
    return events;
}

std::uint64_t fold_bbo(const OrderBook &book) {
    std::uint64_t digest = 0x9E3779B97F4A7C15ULL;
    if (const auto bid = book.best_bid())
        digest ^= bid->price_ticks ^ (bid->quantity << 1U);
    if (const auto ask = book.best_ask())
        digest ^= (ask->price_ticks << 7U) ^ (ask->quantity << 3U);
    return digest;
}

std::uint64_t run_sample(const std::vector<Event> &events, std::uint64_t &digest) {
    auto book = make_book();
#if defined(LLAB_METRICS_CAPTURE)
    llab::metrics::ThreadRing metric_ring(events.size());
#elif defined(LLAB_METRICS_SAMPLED)
    constexpr std::size_t metric_sample_mask = 63;
    llab::metrics::ThreadRing metric_ring((events.size() + metric_sample_mask) /
                                          (metric_sample_mask + 1));
    std::size_t metric_sequence = 0;
#endif
    const auto start = std::chrono::steady_clock::now();
    for (const Event &event : events) {
#if defined(LLAB_METRICS_DISABLED)
        llab::metrics::Scope<false> metric_scope(nullptr, 0);
        book.apply(event);
#elif defined(LLAB_METRICS_SAMPLED)
        if ((metric_sequence++ & metric_sample_mask) == 0) {
            llab::metrics::Scope<true> metric_scope(&metric_ring, 1);
            book.apply(event);
        } else {
            book.apply(event);
        }
#elif defined(LLAB_METRICS_CAPTURE)
        llab::metrics::Scope<true> metric_scope(&metric_ring, 1);
        book.apply(event);
#else
        book.apply(event);
#endif
#if defined(LLAB_READ_BBO)
        digest ^= fold_bbo(book) + 0x9E3779B97F4A7C15ULL + (digest << 6U) + (digest >> 2U);
#endif
    }
    const auto end = std::chrono::steady_clock::now();
#if defined(LLAB_METRICS_CAPTURE)
    if (metric_ring.used() != events.size() || metric_ring.dropped() != 0)
        throw std::runtime_error("metrics capture incomplete");
#elif defined(LLAB_METRICS_SAMPLED)
    if (metric_ring.used() != (events.size() + metric_sample_mask) / (metric_sample_mask + 1) ||
        metric_ring.dropped() != 0)
        throw std::runtime_error("metrics sampled capture incomplete");
#endif
    if (book.live_order_count() != 0 || book.best_bid() || book.best_ask())
        throw std::runtime_error("mixed lifecycle did not restore empty book");
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

void verify_trace(const std::vector<Event> &events) {
    auto book = make_book();
    for (const Event &event : events)
        book.apply(event);
    if (book.live_order_count() != 0 || book.best_bid() || book.best_ask())
        throw std::runtime_error("mixed lifecycle did not restore empty book");
}

std::size_t rank(std::size_t count, long double percentile) {
    return static_cast<std::size_t>(std::ceil(percentile * count)) - 1;
}
} // namespace

int main(int argc, char **argv) {
    std::string raw_path;
    bool verify_only = false;
    if (argc == 3 && std::string_view(argv[1]) == "--raw")
        raw_path = argv[2];
    else if (argc == 2 && std::string_view(argv[1]) == "--verify")
        verify_only = true;
    else if (argc != 1) {
        std::cerr << "usage: mixed_lifecycle [--raw PATH | --verify]\n";
        return 2;
    }

    const auto events = make_trace();
    if (verify_only) {
        verify_trace(events);
        return 0;
    }
    const auto layout_book = make_book();
    std::uint64_t digest = 0;
    for (std::size_t sample = 0; sample < kWarmupSamples; ++sample)
        (void)run_sample(events, digest);
    std::vector<std::uint64_t> samples;
    samples.reserve(kMeasuredSamples);
    for (std::size_t sample = 0; sample < kMeasuredSamples; ++sample)
        samples.push_back(run_sample(events, digest));
    if (!raw_path.empty()) {
        std::ofstream raw(raw_path);
        if (!raw)
            throw std::runtime_error("cannot write raw samples");
        raw << "sample_index,duration_ns\n";
        for (std::size_t index = 0; index < samples.size(); ++index)
            raw << index << ',' << samples[index] << '\n';
    }

    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    long double total = 0;
    for (const auto sample : samples)
        total += sample;
    const auto p50 = sorted[rank(sorted.size(), 0.50L)];
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "format: llab_benchmark_v1\nbenchmark: stream_mixed_lifecycle_" << kVersion
#if defined(LLAB_READ_BBO)
              << "_with_bbo_v1\n";
#else
              << "_v1\n";
#endif
    std::cout << "language: cpp\nevent_size_bytes: " << sizeof(Event)
              << "\norder_book_size_bytes: " << sizeof(OrderBook)
              << "\norder_index_capacity: " << layout_book.order_index_capacity()
              << "\nevents_per_sample: " << events.size() << "\nwarmup_samples: " << kWarmupSamples
              << "\nmeasured_samples: " << kMeasuredSamples << "\nmin_ns: " << sorted.front()
              << "\np50_ns: " << p50 << "\np90_ns: " << sorted[rank(sorted.size(), 0.90L)]
              << "\np99_ns: " << sorted[rank(sorted.size(), 0.99L)] << "\nmax_ns: " << sorted.back()
              << "\nmean_ns: " << total / samples.size()
              << "\np50_ns_per_event: " << static_cast<long double>(p50) / events.size()
              << "\np50_events_per_s: "
              << static_cast<long double>(events.size()) * 1'000'000'000.0L / p50
              << "\nresult_digest: " << digest << '\n';
}
