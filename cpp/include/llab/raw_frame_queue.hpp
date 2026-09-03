#pragma once

#include "llab/raw_frame_record.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace llab {

enum class RawFramePushResult {
    Queued,
    Full,
    PayloadTooLarge,
};

struct RawFrameHeader {
    std::uint64_t capture_index;
    std::uint64_t monotonic_ns;
    std::int64_t utc_ns;
    std::uint64_t connection_id;
    FrameDirection direction;
    FrameKind kind;
};

class RawFrameSpscQueue {
public:
    RawFrameSpscQueue(const std::size_t slot_count, const std::size_t payload_capacity)
        : slots_(slot_count), payload_capacity_(payload_capacity) {
        if (slot_count < 2 || payload_capacity == 0) {
            throw std::invalid_argument("raw frame queue requires at least two non-empty slots");
        }
        for (auto& slot : slots_) {
            slot.payload.resize(payload_capacity_);
        }
    }

    [[nodiscard]] RawFramePushResult try_push(
        const RawFrameHeader header, const std::span<const std::uint8_t> payload) noexcept {
        if (payload.size() > payload_capacity_) {
            return RawFramePushResult::PayloadTooLarge;
        }

        const auto write = write_index_.load(std::memory_order_relaxed);
        const auto next = increment(write);
        if (next == read_index_.load(std::memory_order_acquire)) {
            return RawFramePushResult::Full;
        }

        auto& slot = slots_[write];
        slot.header = header;
        slot.payload_size = payload.size();
        std::copy(payload.begin(), payload.end(), slot.payload.begin());
        write_index_.store(next, std::memory_order_release);
        return RawFramePushResult::Queued;
    }

    [[nodiscard]] bool try_pop_into(RawFrameRecord& out) noexcept {
        const auto read = read_index_.load(std::memory_order_relaxed);
        if (read == write_index_.load(std::memory_order_acquire)) {
            return false;
        }

        const auto& slot = slots_[read];
        if (out.payload.capacity() < slot.payload_size) {
            return false;
        }

        out.capture_index = slot.header.capture_index;
        out.monotonic_ns = slot.header.monotonic_ns;
        out.connection_id = slot.header.connection_id;
        out.utc_ns = slot.header.utc_ns;
        out.direction = slot.header.direction;
        out.kind = slot.header.kind;
        out.payload.clear();
        out.payload.insert(out.payload.end(), slot.payload.begin(), slot.payload.begin() + slot.payload_size);
        read_index_.store(increment(read), std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t payload_capacity() const noexcept { return payload_capacity_; }

private:
    struct Slot {
        RawFrameHeader header{};
        std::size_t payload_size = 0;
        std::vector<std::uint8_t> payload;
    };

    [[nodiscard]] std::size_t increment(const std::size_t index) const noexcept {
        return index + 1 == slots_.size() ? 0 : index + 1;
    }

    std::vector<Slot> slots_;
    std::size_t payload_capacity_;
    alignas(64) std::atomic<std::size_t> write_index_ = 0;
    alignas(64) std::atomic<std::size_t> read_index_ = 0;
};

}  // namespace llab
