#include "llab/raw_frame_queue.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace {
constexpr std::uint64_t events = 1'000'000;
constexpr std::size_t samples = 20;
constexpr std::size_t payload_capacity = 4096;
constexpr int producer_cpu = 0;
constexpr int consumer_cpu = 2;

double rank(std::vector<double> values, const double percentile) {
    std::sort(values.begin(), values.end());
    return values[static_cast<std::size_t>(std::ceil(values.size() * percentile)) - 1];
}

void spin_pause() {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

bool pin_current_thread(const int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
}
}  // namespace

int main() {
    std::vector<double> durations;
    durations.reserve(samples);
    for (std::size_t sample = 0; sample < samples; ++sample) {
        (void)sample;
        llab::RawFrameSpscQueue queue(1024, payload_capacity);
        std::atomic<bool> start = false;
        std::atomic<unsigned int> ready = 0;
        std::atomic<std::uint64_t> consumed = 0;
        std::thread producer([&] {
            if (!pin_current_thread(producer_cpu)) std::terminate();
            ready.fetch_add(1, std::memory_order_release);
            const std::array<std::uint8_t, 64> small{};
            const std::array<std::uint8_t, 2048> large{};
            while (!start.load(std::memory_order_acquire)) spin_pause();
            for (std::uint64_t index = 0; index < events; ++index) {
                const auto payload = index % 16 == 0 ? std::span<const std::uint8_t>(large) : std::span<const std::uint8_t>(small);
                const llab::RawFrameHeader header{index, index, 0, 1, llab::FrameDirection::Inbound, llab::FrameKind::Text};
                while (queue.try_push(header, payload) != llab::RawFramePushResult::Queued) spin_pause();
            }
        });
        std::thread consumer([&] {
            if (!pin_current_thread(consumer_cpu)) std::terminate();
            ready.fetch_add(1, std::memory_order_release);
            llab::RawFrameRecord output{0, 0, 0, 0, llab::FrameDirection::Inbound, llab::FrameKind::Text, {}};
            output.payload.reserve(payload_capacity);
            while (!start.load(std::memory_order_acquire)) spin_pause();
            for (std::uint64_t expected = 0; expected < events; ++expected) {
                while (!queue.try_pop_into(output)) spin_pause();
                if (output.capture_index != expected) std::terminate();
            }
            consumed.store(events, std::memory_order_release);
        });
        while (ready.load(std::memory_order_acquire) != 2) spin_pause();
        const auto begun = std::chrono::steady_clock::now();
        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();
        if (consumed.load(std::memory_order_acquire) != events) std::terminate();
        durations.push_back(std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begun).count() / events);
    }
    std::cout << std::fixed << std::setprecision(3)
              << "samples=" << samples << " events_per_sample=" << events
              << " producer_cpu=" << producer_cpu << " consumer_cpu=" << consumer_cpu
              << " payload=15x64B+1x2048B p50_ns_per_message=" << rank(durations, 0.50)
              << " p99_ns_per_message=" << rank(durations, 0.99) << '\n';
}
