#pragma once
#include "sdk.h"
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

class SessionBase {
    virtual ~SessionBase() = default;
    virtual void Run() = 0;
};

template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
    public:
    Session(tcp::socket socket, RequestHandler handler) : socket_(std::move(socket), 
                                                          handler_(std::move(handler))) {}

	private:
    void DoRead() {
        http::async_read(socket_, buffer_, request_,
            [self = this->shared_from_this()](beast::error_code ec, std::size_t) {
                if (!ec) self->OnRead();
            });
    }
    void OnRead() {
        auto response = handler_(request_);
        http::async_write(socket_, response,
            [self = this->shared_from_this()](beast::error_code, std::size_t) {});
    }

    tcp::socket socket_;
    RequestHandler handler_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
        Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler handler)
        : acceptor_(ioc), socket_(ioc), handler_(std::move(handler)) {
        acceptor_.open(endpoint.protocol());
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

    void Run() { DoAccept(); }

private:
    void DoAccept() {
        acceptor_.async_accept(socket_,
            [self = this->shared_from_this()](beast::error_code ec) {
                if (!ec) {
                    std::make_shared<Session<RequestHandler>>(std::move(self->socket_), self->handler_)->Run();
                }
                self->DoAccept();
            });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    RequestHandler handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    std::make_shared<Listener<RequestHandler>>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server
