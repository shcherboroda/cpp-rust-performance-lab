#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

namespace llab::metrics {

struct CycleSample {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    std::uint32_t tag = 0;
    std::uint32_t cpu = 0;
};

class CycleClock {
  public:
    [[nodiscard]] static constexpr bool supported() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        return true;
#else
        return false;
#endif
    }

    [[nodiscard]] static std::uint64_t begin() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_lfence();
        return __rdtsc();
#else
        return 0;
#endif
    }

    [[nodiscard]] static std::uint64_t end(std::uint32_t &cpu) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        unsigned aux = 0;
        const auto value = __rdtscp(&aux);
        _mm_lfence();
        cpu = aux;
        return value;
#else
        cpu = 0;
        return 0;
#endif
    }
};

class ThreadRing {
  public:
    explicit ThreadRing(std::size_t capacity) : samples_(capacity) {
        if (capacity == 0)
            throw std::invalid_argument("metric ring capacity");
    }

    void record(std::uint32_t tag, std::uint64_t begin, std::uint64_t end,
                std::uint32_t cpu) noexcept {
        if (used_ == samples_.size()) {
            ++dropped_;
            return;
        }
        samples_[used_++] = {begin, end, tag, cpu};
    }

    [[nodiscard]] const std::vector<CycleSample> &storage() const noexcept { return samples_; }
    [[nodiscard]] std::size_t used() const noexcept { return used_; }
    [[nodiscard]] std::size_t dropped() const noexcept { return dropped_; }
    void reset() noexcept { used_ = 0; dropped_ = 0; }

  private:
    std::vector<CycleSample> samples_;
    std::size_t used_ = 0;
    std::size_t dropped_ = 0;
};

template <bool Enabled> class Scope;

template <> class Scope<false> {
  public:
    constexpr Scope(ThreadRing *, std::uint32_t) noexcept {}
};

template <> class Scope<true> {
  public:
    Scope(ThreadRing *ring, std::uint32_t tag) noexcept
        : ring_(ring), tag_(tag), begin_(CycleClock::begin()) {
    }
    ~Scope() noexcept {
        if (ring_) {
            std::uint32_t cpu = 0;
            ring_->record(tag_, begin_, CycleClock::end(cpu), cpu);
        }
    }

  private:
    ThreadRing *ring_;
    std::uint32_t tag_;
    std::uint64_t begin_;
};

} // namespace llab::metrics
