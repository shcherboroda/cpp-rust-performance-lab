#include "llab/raw_frame_record.hpp"

#include <stdexcept>

int main() {
    llab::RawFrameRecord record{7, 9, 11, -1, llab::FrameDirection::Inbound,
                                llab::FrameKind::Text, {'[', '1', ',', '2', ']'}};
    auto bytes = llab::encode_raw_frame(record);
    if (llab::decode_raw_frame(bytes) != std::optional{record})
        throw std::runtime_error("raw-frame round trip failed");
    bytes[0] ^= 1;
    if (llab::decode_raw_frame(bytes).has_value())
        throw std::runtime_error("raw-frame checksum failed");

    std::vector<std::uint8_t> reusable(llab::raw_frame_header_size + llab::raw_frame_max_payload + 4);
    reusable.clear();
    const auto* allocation = reusable.data();
    llab::encode_raw_frame_into(record, reusable);
    if (llab::decode_raw_frame(reusable) != std::optional{record})
        throw std::runtime_error("raw-frame reusable-buffer round trip failed");
    if (reusable.data() != allocation)
        throw std::runtime_error("raw-frame encoder allocated despite sufficient capacity");
}
