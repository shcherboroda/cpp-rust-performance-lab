#include "llab/raw_frame_queue.hpp"

#include <array>
#include <stdexcept>

int main() {
    llab::RawFrameSpscQueue queue(3, 8);
    const llab::RawFrameHeader first{1, 2, 3, 4, llab::FrameDirection::Inbound, llab::FrameKind::Text};
    const llab::RawFrameHeader second{5, 6, 7, 8, llab::FrameDirection::Outbound, llab::FrameKind::Control};
    const std::array<std::uint8_t, 3> first_payload{'a', 'b', 'c'};
    const std::array<std::uint8_t, 2> second_payload{'d', 'e'};

    if (queue.try_push(first, first_payload) != llab::RawFramePushResult::Queued ||
        queue.try_push(second, second_payload) != llab::RawFramePushResult::Queued ||
        queue.try_push(first, first_payload) != llab::RawFramePushResult::Full) {
        throw std::runtime_error("queue capacity contract failed");
    }

    llab::RawFrameRecord output{0, 0, 0, 0, llab::FrameDirection::Inbound, llab::FrameKind::Text, {}};
    output.payload.reserve(queue.payload_capacity());
    if (!queue.try_pop_into(output) || output.capture_index != 1 ||
        !std::equal(output.payload.begin(), output.payload.end(), first_payload.begin(), first_payload.end())) {
        throw std::runtime_error("first queued frame changed");
    }
    if (!queue.try_pop_into(output) || output.capture_index != 5 ||
        !std::equal(output.payload.begin(), output.payload.end(), second_payload.begin(), second_payload.end())) {
        throw std::runtime_error("second queued frame changed");
    }
    if (queue.try_pop_into(output)) {
        throw std::runtime_error("empty queue returned a frame");
    }

    const std::array<std::uint8_t, 9> oversize{};
    if (queue.try_push(first, oversize) != llab::RawFramePushResult::PayloadTooLarge) {
        throw std::runtime_error("oversize frame was accepted");
    }
}
