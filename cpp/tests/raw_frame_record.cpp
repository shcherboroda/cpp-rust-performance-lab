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
}
