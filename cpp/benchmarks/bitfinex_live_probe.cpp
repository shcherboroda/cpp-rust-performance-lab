#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

namespace {

double elapsed_ms(const std::chrono::steady_clock::time_point started) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

}  // namespace

int main() {
    try {
        constexpr std::string_view host = "api-pub.bitfinex.com";
        constexpr std::string_view service = "443";
        constexpr std::string_view target = "/ws/2";
        constexpr std::string_view subscription =
            R"({"event":"subscribe","channel":"book","symbol":"tBTCUSD","prec":"R0","freq":"F0","len":"25"})";

        const auto started = std::chrono::steady_clock::now();
        net::io_context context;
        ssl::context tls_context(ssl::context::tls_client);
        tls_context.set_default_verify_paths();
        tls_context.set_verify_mode(ssl::verify_peer);

        tcp::resolver resolver(context);
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> socket(context, tls_context);
        const auto endpoints = resolver.resolve(host, service);
        beast::get_lowest_layer(socket).connect(endpoints);

        if (!SSL_set_tlsext_host_name(socket.next_layer().native_handle(), host.data())) {
            throw beast::system_error(
                static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
        }

        socket.next_layer().handshake(ssl::stream_base::client);
        socket.handshake(host, target);
        std::cout << std::fixed << std::setprecision(3) << "connect_ms=" << elapsed_ms(started) << '\n';

        socket.write(net::buffer(subscription));
        for (std::size_t index = 0; index < 4; ++index) {
            beast::flat_buffer frame;
            socket.read(frame);
            const std::string payload = beast::buffers_to_string(frame.data());
            std::cout << "frame=" << index << " receive_ms=" << elapsed_ms(started)
                      << " bytes=" << payload.size() << " payload=" << payload << '\n';
        }

        socket.close(websocket::close_code::normal);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bitfinex_live_probe failed: " << error.what() << '\n';
        return 1;
    }
}
