#include "sdk.h"

#include "json_loader.h"
#include "json_logger.h"
#include "model.h"
#include "player.h"
#include "request_handler.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <thread>
#include <optional>
#include <vector>

using namespace std::literals;
namespace net = boost::asio;

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

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-data-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }

    json_logger::InitLogger();

    try {
        std::filesystem::path game_config_file_path = argv[1];
        std::string static_data_dir_path = argv[2];

        model::Game game = json_loader::LoadGame(game_config_file_path);
        player::Players players;

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, [[maybe_unused]] int) {
            if (!ec) ioc.stop();
        });

        auto api_strand = net::make_strand(ioc);
        auto handler = std::make_shared<http_handler::RequestHandler>(game, players, static_data_dir_path, api_strand);

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [handler](auto&& req, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        json_logger::LogData("server started"sv, boost::json::object{{"port", port}, {"address", address.to_string()}});

        std::vector<std::thread> workers;
        for (unsigned i = 0; i < std::max(1u, num_threads); ++i) {
            workers.emplace_back([&ioc] { ioc.run(); });
        }
        for (auto& w : workers) w.join();

        json_logger::LogData("server exited"sv, boost::json::object{{"code", 0}});
    } catch (const std::exception& ex) {
        json_logger::LogData("server exited"sv, boost::json::object{{"code", EXIT_FAILURE}, {"exception", ex.what()}});
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}