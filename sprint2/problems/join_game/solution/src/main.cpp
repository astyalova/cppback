#include "sdk.h"
#include "json_loader.h"
#include "request_handler.h"
#include "json_logger.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/core/detail/string_view.hpp>
#include <iostream>
#include <thread>

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

} // namespace

int main(int argc, const char* argv[]) {
    json_logger::InitLogger();

    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-data-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }

    int exit_code = 0;
    try {
        model::Game game = json_loader::LoadGame(argv[1]);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        player::Players players;
        http_handler::RequestHandler::Strand api_strand{ioc.get_executor()};
        http_handler::RequestHandler handler{game, players, argv[2], api_strand};
        http_handler::LoggingRequestHandler logging_handler{handler};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        json_logger::LogData("server started",
            boost::json::object{{"port", port}, {"address", address.to_string()}});

        http_server::ServeHttp(ioc, net::ip::tcp::endpoint{net::ip::tcp::v4(), port},
            [&](auto&& req, auto&& send) {
                logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
            });

        RunWorkers(std::max(1u, num_threads), [&ioc]{ ioc.run(); });

    } catch (const std::exception& ex) {
        exit_code = EXIT_FAILURE;
        json_logger::LogData("server exited",
            boost::json::object{{"code", exit_code}, {"exception", ex.what()}});
    }

    json_logger::LogData("server exited",
        boost::json::object{{"code", exit_code}, {"exception", exit_code == 0 ? nullptr : "unknown"}});

    return exit_code;
}
