#include "sdk.h"

#include "application.h"
#include "json_loader.h"
#include "json_logger.h"
#include "model.h"
#include "player.h"
#include "request_handler.h"
#include "ticket.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/core/detail/string_view.hpp>
#include <fstream>
#include <iostream>
#include <thread>
#include <optional>
#include <vector>

using namespace std::literals;
namespace net = boost::asio;

struct Args {
    int period_ticket;
    std::string path_to_file;
    std::string path_to_catalogue;
    bool spawn;
}; 

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options:"};

    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period, t", po::value(&argc.period_ticket)->value_name("milliseconds"), "set tick period")
        ("config-file, c", po::value(&argc.path_to_file)->value_name("file"), "set config file path")
        ("www-root, w", po::value(&argc.path_to_catalogue)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions ");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
            std::cout << desc;
            return std::nullopt;
        }
        if (!vm.contains("config-file"s)) {
            throw std::runtime_error("Error: configuration file path is not specified"s);
        }
        if (!vm.contains("www-root"s)) {
            throw std::runtime_error("Error: static files root directory is not specified"s);
        }
        if (!vm.contains("tick-period"s)) {
            args.period_ticket = -1;
        }
        args.randomize_spawn_points = vm.contains("randomize-spawn-points"s);

    return args;
}

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
        if (auto args = ParseCommandLine(argc, argv)) {
            std::filesystem::path config_file = args->path_to_file;
            std::string www_root = args->path_to_catalogue;

            Application app(json_parser::LoadGame(config_file),
                            args->spawn,
                            args->period_ticket >= 0);

            const unsigned num_threads = std::thread::hardware_concurrency();
            net::io_context ioc(num_threads);

            net::signal_set signals(ioc, SIGINT, SIGTERM);
            signals.async_wait([&ioc](const boost::system::error_code& ec, [[maybe_unused]] int) {
                if (!ec) ioc.stop();
            });

            auto api_strand = net::make_strand(ioc);
            auto handler = std::make_shared<http_handler::RequestHandler>(app, www_root, api_strand);

            auto ticker = std::make_shared<http_handler::Ticker>(api_strand, std::chrono::milliseconds(args->tick_period),
                [&app](std::chrono::milliseconds delta) { 
                    if (app.GetAutoTick()) {
                        app.Tick(delta);
                    }                    
                }
            );
            ticker->Start();

            const auto address = net::ip::make_address("0.0.0.0");
            constexpr unsigned short port = 8080;
            http_server::ServeHttp(ioc, {address, port}, [handler](auto&& req, auto&& send) {
                (*handler)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
            });

            json_logger::LogData("server started"sv, boost::json::object{{"port", port}, {"address", address.to_string()}});

            RunWorkers(std::max(1u, num_threads), [&ioc] {
                ioc.run();
            });
            json_logger::LogData("server exited"sv, boost::json::object{{"code", 0}});
        }
    } catch (const std::exception& ex) {
        json_logger::LogData("server exited"sv, boost::json::object{{"code", EXIT_FAILURE}, {"exception", ex.what()}});
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}