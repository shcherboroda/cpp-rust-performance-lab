#define LLAB_BYBIT_CAPTURE_TEST
#include "../benchmarks/bybit_l2_capture.cpp"

#include <filesystem>
#include <stdexcept>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "llab_async_recorder_drain.llfr";
    std::filesystem::remove(path);
    AsyncRecorder recorder(path.string(), {"orderbook.50.BTCUSDT"});
    for (std::uint64_t index = 0; index < 1000; ++index)
        recorder.record({index, index, 1, 0, llab::FrameDirection::Inbound, llab::FrameKind::Text, {'x'}});
    recorder.close();
    llab::RawFrameCaptureReader reader(path.string());
    for (std::uint64_t index = 0; index < 1000; ++index) {
        const auto record = reader.next();
        if (!record || record->capture_index != index) throw std::runtime_error("recorder lost queued frame during close");
    }
    if (reader.next()) throw std::runtime_error("unexpected recorder frame");
    std::filesystem::remove(path);
}
