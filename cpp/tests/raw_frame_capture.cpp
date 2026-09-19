#include "llab/raw_frame_capture.hpp"

#include <filesystem>
#include <stdexcept>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "llab_raw_capture_test.llfr";
    std::filesystem::remove(path);
    const llab::RawFrameRecord expected{0, 5, 6, 7, llab::FrameDirection::Inbound, llab::FrameKind::Text,
                                        {'{', '}', '\n'}};
    {
        llab::RawFrameCaptureWriter writer(path.string(), {"orderbook.50.BTCUSDT"});
        writer.append(expected);
        writer.close();
    }
    llab::RawFrameCaptureReader reader(path.string());
    if (reader.metadata().topic != "orderbook.50.BTCUSDT" || reader.next() != std::optional{expected} || reader.next().has_value())
        throw std::runtime_error("raw capture round trip failed");
    bool overwrite_rejected = false;
    try { static_cast<void>(llab::RawFrameCaptureWriter(path.string(), {"orderbook.50.BTCUSDT"})); }
    catch (const std::runtime_error&) { overwrite_rejected = true; }
    if (!overwrite_rejected) throw std::runtime_error("completed capture overwrite accepted");
    std::filesystem::remove(path);
}
