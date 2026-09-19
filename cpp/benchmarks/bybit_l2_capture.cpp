#include "llab/bybit_l2_order_book.hpp"
#include "llab/bybit_v5_l2_decoder.hpp"
#include "llab/feed_receive_metrics.hpp"
#include "llab/raw_frame_capture.hpp"
#include "llab/raw_frame_queue.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <openssl/err.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

namespace {

struct Config {
    std::string host = "stream.bybit.com";
    std::string target = "/v5/public/spot";
    std::string symbol = "BTCUSDT";
    std::size_t depth = 50;
    std::size_t max_frames = 100;
    std::optional<std::string> record_path;
    std::optional<std::string> replay_path;
};

std::unordered_map<std::string, std::string> load_dotenv() {
    std::unordered_map<std::string, std::string> values;
    std::ifstream file(".env");
    for (std::string line; std::getline(file, line);) {
        const auto equal = line.find('=');
        if (equal == std::string::npos || line.empty() || line.front() == '#') continue;
        values.emplace(line.substr(0, equal), line.substr(equal + 1));
    }
    return values;
}

std::string env_or(const std::unordered_map<std::string, std::string>& dotenv, const std::string& name,
                   const std::string& fallback) {
    if (const auto* value = std::getenv(name.c_str())) return value;
    if (const auto found = dotenv.find(name); found != dotenv.end()) return found->second;
    return fallback;
}

Config parse_config(const int argc, char** argv) {
    const auto dotenv = load_dotenv();
    Config config;
    config.host = env_or(dotenv, "BYBIT_WS_HOST", config.host);
    config.target = env_or(dotenv, "BYBIT_WS_TARGET", config.target);
    config.symbol = env_or(dotenv, "BYBIT_SYMBOL", config.symbol);
    config.depth = std::stoull(env_or(dotenv, "BYBIT_ORDERBOOK_DEPTH", std::to_string(config.depth)));
    config.max_frames = std::stoull(env_or(dotenv, "BYBIT_MAX_FRAMES", std::to_string(config.max_frames)));
    const auto capture = env_or(dotenv, "BYBIT_CAPTURE_PATH", "");
    if (!capture.empty()) config.record_path = capture;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--record" && i + 1 < argc) config.record_path = argv[++i];
        else if (arg == "--replay" && i + 1 < argc) config.replay_path = argv[++i];
        else if (arg == "--max-frames" && i + 1 < argc) config.max_frames = std::stoull(argv[++i]);
        else if (arg == "--symbol" && i + 1 < argc) config.symbol = argv[++i];
        else if (arg == "--depth" && i + 1 < argc) config.depth = std::stoull(argv[++i]);
        else throw std::invalid_argument("usage: bybit_l2_capture [--record path] [--replay path] [--max-frames N] [--symbol SYMBOL] [--depth N]");
    }
    if (config.depth == 0 || config.max_frames == 0) throw std::invalid_argument("depth and max-frames must be positive");
    return config;
}

std::uint64_t monotonic_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::int64_t utc_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

class AsyncRecorder {
public:
    AsyncRecorder(const std::string& path, const llab::RawCaptureMetadata& metadata)
        : queue_(4096, llab::raw_frame_max_payload), writer_(path, metadata), worker_([this] { run(); }) {}
    ~AsyncRecorder() {
        abort();
    }

    void record(llab::RawFrameRecord record) {
        if (writer_failed_.load(std::memory_order_acquire)) throw std::runtime_error("capture writer failed");
        const llab::RawFrameHeader header{record.capture_index, record.monotonic_ns, record.utc_ns, record.connection_id,
                                          record.direction, record.kind};
        const auto result = queue_.try_push(header, record.payload);
        if (result != llab::RawFramePushResult::Queued)
            throw std::runtime_error(result == llab::RawFramePushResult::Full ? "capture queue overflow" : "capture frame too large");
    }

    void close() {
        if (closed_.exchange(true)) return;
        finalize_.store(true, std::memory_order_release);
        stop_.store(true, std::memory_order_release);
        if (worker_.joinable()) worker_.join();
        if (writer_failed_.load(std::memory_order_acquire)) throw std::runtime_error("capture writer failed");
    }

    void abort() noexcept {
        if (closed_.exchange(true)) return;
        stop_.store(true, std::memory_order_release);
        if (worker_.joinable()) worker_.join();
    }

private:
    void run() noexcept {
        try {
            llab::RawFrameRecord record{};
            record.payload.reserve(queue_.payload_capacity());
            for (;;) {
                if (queue_.try_pop_into(record)) {
                    writer_.append(record);
                    continue;
                }
                if (stop_.load(std::memory_order_acquire)) break;
                std::this_thread::yield();
            }
            if (finalize_.load(std::memory_order_acquire)) writer_.close();
        } catch (...) { writer_failed_.store(true, std::memory_order_release); }
    }
    llab::RawFrameSpscQueue queue_;
    llab::RawFrameCaptureWriter writer_;
    std::atomic<bool> stop_ = false, closed_ = false, finalize_ = false, writer_failed_ = false;
    std::thread worker_;
};

struct Processor {
    Processor(const std::size_t depth, std::string expected_topic, const bool live_metrics)
        : book(depth), expected_topic(std::move(expected_topic)), live_metrics(live_metrics) {}
    void consume(const llab::RawFrameRecord& record) {
        if (record.direction != llab::FrameDirection::Inbound || record.kind != llab::FrameKind::Text) return;
        const auto handoff_ns = live_metrics ? monotonic_ns() - record.monotonic_ns : 0;
        const auto message = llab::bybit_v5::decode({reinterpret_cast<const char*>(record.payload.data()), record.payload.size()});
        if (!message || message->kind == llab::bybit_v5::MessageKind::Control) { metrics.record_frame(llab::FeedFrameClass::Control, record.monotonic_ns, handoff_ns); return; }
        if (message->topic != expected_topic) throw std::runtime_error("market message topic does not match subscribed topic");
        const auto frame_class = message->kind == llab::bybit_v5::MessageKind::Snapshot ? llab::FeedFrameClass::Snapshot : llab::FeedFrameClass::MarketUpdate;
        if (!llab::bybit_v5::apply(*message, book)) throw std::runtime_error("Bybit update-id/capacity gate rejected message; resnapshot required");
        metrics.record_frame(frame_class, record.monotonic_ns, handoff_ns);
    }
    llab::bybit_l2::OrderBook book;
    llab::FeedReceiveMetrics metrics;
    std::string expected_topic;
    bool live_metrics;
};

void print_status(const Processor& processor) {
    const auto metrics = processor.metrics.snapshot();
    std::cout << "snapshots=" << metrics.snapshots << " updates=" << metrics.market_updates
              << " controls=" << metrics.control_frames << " valid=" << processor.book.valid()
              << " levels=" << processor.book.level_count();
    if (const auto bid = processor.book.best_bid()) std::cout << " best_bid_1e8=" << bid->price << ':' << bid->quantity;
    if (const auto ask = processor.book.best_ask()) std::cout << " best_ask_1e8=" << ask->price << ':' << ask->quantity;
    std::cout << '\n';
}

std::size_t depth_from_topic(const std::string& topic) {
    constexpr std::string_view prefix = "orderbook.";
    if (!topic.starts_with(prefix)) throw std::invalid_argument("capture metadata is not an orderbook topic");
    const auto separator = topic.find('.', prefix.size());
    if (separator == std::string::npos || separator == prefix.size() || separator + 1 == topic.size())
        throw std::invalid_argument("malformed orderbook capture topic");
    std::size_t depth = 0;
    const auto [end, error] = std::from_chars(topic.data() + prefix.size(), topic.data() + separator, depth);
    if (error != std::errc{} || end != topic.data() + separator || depth == 0)
        throw std::invalid_argument("invalid capture orderbook depth");
    return depth;
}

int replay(const Config& config) {
    llab::RawFrameCaptureReader reader(*config.replay_path);
    Processor processor(depth_from_topic(reader.metadata().topic), reader.metadata().topic, false);
    while (const auto record = reader.next()) processor.consume(*record);
    print_status(processor);
    return processor.book.valid() ? 0 : 2;
}

int live(const Config& config) {
    net::io_context context;
    ssl::context tls_context(ssl::context::tls_client);
    tls_context.set_default_verify_paths();
    tls_context.set_verify_mode(ssl::verify_peer);
    tls_context.set_verify_callback(ssl::host_name_verification(config.host));
    tcp::resolver resolver(context);
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> socket(context, tls_context);
    auto timeout = websocket::stream_base::timeout::suggested(beast::role_type::client);
    timeout.handshake_timeout = std::chrono::seconds(10);
    timeout.idle_timeout = std::chrono::seconds(30);
    timeout.keep_alive_pings = true;
    socket.set_option(timeout);
    beast::get_lowest_layer(socket).expires_after(std::chrono::seconds(10));
    const auto endpoints = resolver.resolve(config.host, "443");
    beast::get_lowest_layer(socket).connect(endpoints);
    if (!SSL_set_tlsext_host_name(socket.next_layer().native_handle(), config.host.c_str()))
        throw beast::system_error(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
    socket.next_layer().handshake(ssl::stream_base::client);
    beast::get_lowest_layer(socket).expires_never();
    socket.handshake(config.host, config.target);
    const std::string topic = "orderbook." + std::to_string(config.depth) + "." + config.symbol;
    const std::string subscription = "{\"op\":\"subscribe\",\"args\":[\"" + topic + "\"]}";
    std::optional<AsyncRecorder> recorder;
    if (config.record_path) recorder.emplace(*config.record_path, llab::RawCaptureMetadata{topic});
    Processor processor(config.depth, topic, true);
    std::uint64_t index = 0;
    const auto record = [&](const llab::FrameDirection direction, const llab::FrameKind kind, const std::uint64_t captured_monotonic_ns,
                            const std::int64_t captured_utc_ns, const std::string_view payload) {
        llab::RawFrameRecord frame{index++, captured_monotonic_ns, 1, captured_utc_ns, direction, kind,
                                   {payload.begin(), payload.end()}};
        if (recorder) recorder->record(frame);
        return frame;
    };
    const auto outbound = record(llab::FrameDirection::Outbound, llab::FrameKind::Text, monotonic_ns(), utc_ns(), subscription);
    socket.write(net::buffer(outbound.payload));
    for (std::size_t received = 0; received < config.max_frames;) {
        beast::flat_buffer buffer;
        socket.read(buffer);
        const auto received_monotonic_ns = monotonic_ns();
        const auto received_utc_ns = utc_ns();
        const auto kind = socket.got_text() ? llab::FrameKind::Text : llab::FrameKind::Binary;
        const auto payload = beast::buffers_to_string(buffer.data());
        auto frame = record(llab::FrameDirection::Inbound, kind, received_monotonic_ns, received_utc_ns, payload);
        processor.consume(frame);
        ++received;
    }
    if (recorder) recorder->close();
    socket.close(websocket::close_code::normal);
    print_status(processor);
    return processor.book.valid() ? 0 : 2;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto config = parse_config(argc, argv);
        return config.replay_path ? replay(config) : live(config);
    } catch (const std::exception& error) {
        std::cerr << "bybit_l2_capture failed: " << error.what() << '\n';
        return 1;
    }
}
