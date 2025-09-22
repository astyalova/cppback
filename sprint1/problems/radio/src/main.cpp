#include "audio.h"

#include <iostream>
#include <string>
#include <string_view>
#include <boost/asio.hpp>

using namespace std::literals;
namespace net = boost::asio;
using net::ip::udp;

void StartServer(uint16_t port) {
    static const size_t max_buffer_size = 1024;
    Player player(ma_format_u8, 1);
    
    try {
        boost::asio::io_context io_context;

        udp::socket socket(io_context, udp::endpoint(udp::v4(), port));
        std::vector<uint8_t> buf(max_buffer_size);

        while (true) {
            udp::endpoint sender;
            auto size = socket.receive_from(net::buffer(buf), sender);

            size_t frames = size / player.GetFrameSize();

            if (frames > 0) {
                player.PlayBuffer(reinterpret_cast<const char*>(buf.data()), frames, std::chrono::milliseconds(200));
            }
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

void StartClient(uint16_t port) {
    static const size_t max_buffer_size = 1024;
    Recorder recorder(ma_format_u8, 1);

    net::io_context io_context;
    udp::socket socket(io_context, udp::v4());

    udp::endpoint server(net::ip::make_address("127.0.0.1"), port);

    while (true) {
        std::cout << "Нажмите Enter для записи...\n";
        std::string tmp;
        std::getline(std::cin, tmp);

        // записываем звук
        auto result = recorder.Record(65000, 1.5s);

        // вычисляем размер байт
        size_t bytes = result.frames * recorder.GetFrameSize();

        const uint8_t* data = reinterpret_cast<const uint8_t*>(result.data.data());
        size_t sent = 0;

        // отправляем чанками, чтобы не превысить MTU
        while (sent < bytes) {
            size_t chunk = std::min(max_buffer_size, bytes - sent);
            socket.send_to(net::buffer(data + sent, chunk), server);
            sent += chunk;
        }
    }
}

int main(int argc, char** argv) {

    uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
    if(std::string(argv[1]) == "server") {
        StartServer(port);
    } else if(std::string(argv[1]) == "client") {
        StartClient(port);
    } else {
        std::cerr << "Unknown mode\n";
        return 1;
    }

    return 0;
}
