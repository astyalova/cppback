#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <iostream> 
#include <optional>
#include <thread>

#define BOOST_BEAST_USE_STD_STRING_VIEW
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp> 

namespace beast = boost::beast;
namespace http = beast::http;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;

std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code ec;
    StringRequest req;

    http::read(socket, buffer, req, ec);

    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }
    if (ec) {
        throw std::runtime_error("Failed to read request: "s.append(ec.message()));
    }
    return req;
}

void DumpRequest(const StringRequest& req) {
    std::cout << req.method_string() << ' ' << req.target() << std::endl;
    for (const auto& header : req) {
        std::cout << "  "sv << header.name_string() << ": "sv << header.value() << std::endl;
    }
}

void HandleConnection(tcp::socket socket) {
    beast::flat_buffer buffer;

    while (auto request = ReadRequest(socket, buffer)) {
        DumpRequest(*request);

        StringResponse response;

        if (request->method() == http::verb::get || request->method() == http::verb::head) {
            std::string target{request->target().data(), request->target().size()};
            if (!target.empty() && target[0] == '/') {
                target.erase(0, 1);  
            }
            std::string body = "Hello, " + target;

            response = StringResponse(http::status::ok, request->version());
            response.set(http::field::content_type, "text/html"sv);
            response.content_length(body.size());
            response.keep_alive(request->keep_alive());

            if (request->method() == http::verb::get) {
                response.body() = body;
            }

        } else {
            std::string body = "Invalid method.";

            response = StringResponse(http::status::method_not_allowed, request->version());
            response.set(http::field::content_type, "text/html"sv);
            response.set(http::field::allow, "GET, HEAD"sv);
            response.content_length(body.size());
            response.keep_alive(request->keep_alive());
            response.body() = body;
        }

        http::write(socket, response);

        if (response.need_eof()) {
            break;
        }
    }    
}

int main() {
    try {
        net::io_context ioc;

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        tcp::acceptor acceptor(ioc, {address, port});
        std::cout << "Server has started..."sv << std::endl;

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);

            std::thread t(
                [](tcp::socket sock) { HandleConnection(std::move(sock)); },
                std::move(socket)
            );
            t.detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
