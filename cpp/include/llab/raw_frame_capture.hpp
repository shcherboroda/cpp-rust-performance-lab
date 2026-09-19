#pragma once

#include "llab/raw_frame_record.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace llab {

// LLFR v2 is self-describing and atomically published. A .partial file is
// deliberately not readable: successful close writes this trailer then renames
// the file into place.
inline constexpr std::array<std::uint8_t, 8> raw_capture_magic{'L', 'L', 'F', 'R', 0, 0, 0, 2};
inline constexpr std::array<std::uint8_t, 8> raw_capture_complete_magic{'L', 'L', 'F', 'R', 'E', 'N', 'D', 2};
constexpr std::size_t raw_capture_max_metadata = 4096;

struct RawCaptureMetadata { std::string topic; };

class RawFrameCaptureWriter {
public:
    RawFrameCaptureWriter(const std::string& path, const RawCaptureMetadata& metadata)
        : final_path_(path), partial_path_(path + ".partial." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())) {
        if (metadata.topic.empty() || metadata.topic.size() > raw_capture_max_metadata)
            throw std::invalid_argument("invalid raw-capture topic metadata");
        const auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
        if (std::filesystem::exists(final_path_)) throw std::runtime_error("refusing to overwrite completed raw-frame capture: " + final_path_);
        file_.open(partial_path_, std::ios::binary | std::ios::trunc);
        if (!file_) throw std::runtime_error("cannot open raw-frame capture for writing: " + partial_path_);
        file_.write(reinterpret_cast<const char*>(raw_capture_magic.data()), raw_capture_magic.size());
        std::vector<std::uint8_t> metadata_size;
        append_le(metadata_size, static_cast<std::uint32_t>(metadata.topic.size()));
        file_.write(reinterpret_cast<const char*>(metadata_size.data()), static_cast<std::streamsize>(metadata_size.size()));
        file_.write(metadata.topic.data(), static_cast<std::streamsize>(metadata.topic.size()));
        if (!file_) throw std::runtime_error("cannot write raw-frame capture header");
    }

    void append(const RawFrameRecord& record) {
        if (closed_) throw std::logic_error("cannot append a closed raw-frame capture");
        encode_raw_frame_into(record, encoded_);
        file_.write(reinterpret_cast<const char*>(encoded_.data()), static_cast<std::streamsize>(encoded_.size()));
        if (!file_) throw std::runtime_error("cannot append raw-frame capture");
    }

    void close() {
        if (closed_) return;
        file_.write(reinterpret_cast<const char*>(raw_capture_complete_magic.data()), raw_capture_complete_magic.size());
        file_.flush();
        file_.close();
        if (!file_) throw std::runtime_error("cannot finalize raw-frame capture");
        if (std::filesystem::exists(final_path_)) throw std::runtime_error("raw-frame capture appeared while recording: " + final_path_);
        std::filesystem::rename(partial_path_, final_path_);
        closed_ = true;
    }

private:
    std::string final_path_, partial_path_;
    std::ofstream file_;
    std::vector<std::uint8_t> encoded_;
    bool closed_ = false;
};

class RawFrameCaptureReader {
public:
    explicit RawFrameCaptureReader(const std::string& path) : file_(path, std::ios::binary) {
        if (!file_) throw std::runtime_error("cannot open raw-frame capture: " + path);
        std::array<std::uint8_t, raw_capture_magic.size()> magic{};
        file_.read(reinterpret_cast<char*>(magic.data()), magic.size());
        if (file_.gcount() != static_cast<std::streamsize>(magic.size()) || magic != raw_capture_magic)
            throw std::runtime_error("not an LLFR v2 completed raw-frame capture: " + path);
        std::array<std::uint8_t, 4> size_bytes{};
        file_.read(reinterpret_cast<char*>(size_bytes.data()), size_bytes.size());
        if (file_.gcount() != static_cast<std::streamsize>(size_bytes.size())) throw std::runtime_error("truncated capture metadata");
        const auto metadata_size = read_le<std::uint32_t>(size_bytes, 0);
        if (metadata_size == 0 || metadata_size > raw_capture_max_metadata) throw std::runtime_error("invalid capture metadata length");
        metadata_.topic.resize(metadata_size);
        file_.read(metadata_.topic.data(), static_cast<std::streamsize>(metadata_size));
        if (file_.gcount() != static_cast<std::streamsize>(metadata_size)) throw std::runtime_error("truncated capture metadata");
        file_.seekg(0, std::ios::end);
        const auto size = file_.tellg();
        if (size < static_cast<std::streamoff>(raw_capture_complete_magic.size())) throw std::runtime_error("incomplete raw-frame capture");
        data_end_ = size - static_cast<std::streamoff>(raw_capture_complete_magic.size());
        file_.seekg(data_end_);
        std::array<std::uint8_t, raw_capture_complete_magic.size()> complete{};
        file_.read(reinterpret_cast<char*>(complete.data()), complete.size());
        if (file_.gcount() != static_cast<std::streamsize>(complete.size()) || complete != raw_capture_complete_magic)
            throw std::runtime_error("incomplete raw-frame capture");
        file_.clear();
        file_.seekg(static_cast<std::streamoff>(raw_capture_magic.size() + size_bytes.size() + metadata_size));
    }

    [[nodiscard]] const RawCaptureMetadata& metadata() const noexcept { return metadata_; }

    [[nodiscard]] std::optional<RawFrameRecord> next() {
        if (file_.tellg() == data_end_) return std::nullopt;
        if (file_.tellg() > data_end_) throw std::runtime_error("capture record overruns completion marker");
        std::array<std::uint8_t, raw_frame_header_size> header{};
        file_.read(reinterpret_cast<char*>(header.data()), header.size());
        if (file_.gcount() != static_cast<std::streamsize>(header.size())) throw std::runtime_error("truncated raw-frame capture header");
        const auto payload_size = read_le<std::uint32_t>(header, 34);
        if (payload_size > raw_frame_max_payload) throw std::runtime_error("raw-frame payload exceeds limit");
        encoded_.assign(header.begin(), header.end());
        encoded_.resize(raw_frame_header_size + payload_size + 4);
        file_.read(reinterpret_cast<char*>(encoded_.data() + raw_frame_header_size), static_cast<std::streamsize>(payload_size + 4));
        if (file_.gcount() != static_cast<std::streamsize>(payload_size + 4)) throw std::runtime_error("truncated raw-frame capture payload");
        if (file_.tellg() > data_end_) throw std::runtime_error("capture record overruns completion marker");
        const auto decoded = decode_raw_frame(encoded_);
        if (!decoded) throw std::runtime_error("corrupt raw-frame capture record");
        if (decoded->capture_index != next_capture_index_++) throw std::runtime_error("non-contiguous raw-frame capture index");
        return decoded;
    }

private:
    std::ifstream file_;
    RawCaptureMetadata metadata_;
    std::streamoff data_end_ = 0;
    std::vector<std::uint8_t> encoded_;
    std::uint64_t next_capture_index_ = 0;
};

}  // namespace llab
