#include "sdk.h"

#include "application.h"
#include "json_loader.h"
#include "json_logger.h"
#include "model.h"
#include "player.h"
#include "request_handler.h"
#include "ticker.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/core/detail/string_view.hpp>
#include <cstdlib>
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
        ("tick-period, t", po::value(&args.period_ticket)->value_name("milliseconds"), "set tick period")
        ("config-file, c", po::value(&args.path_to_file)->value_name("file"), "set config file path")
        ("www-root, w", po::value(&args.path_to_catalogue)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions ");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help")) {
            std::cout << desc;
            return std::nullopt;
        }
        if (!vm.contains("config-file")) {
            throw std::runtime_error("Error: configuration file path is not specified");
        }
        if (!vm.contains("www-root"s)) {
            throw std::runtime_error("Error: static files root directory is not specified");
        }
        if (!vm.contains("tick-period")) {
            args.period_ticket = -1;
        }
        args.spawn = vm.contains("randomize-spawn-points");

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
    json_logger::InitLogger();

    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            std::filesystem::path path_to_file = args->path_to_file;
            std::string www_root = args->path_to_catalogue;

            // Get database URL from environment variable
            const char* db_url = std::getenv("GAME_DB_URL");
            if (!db_url) {
                throw std::runtime_error("GAME_DB_URL environment variable is not specified");
            }

            Application app(json_loader::LoadGame(path_to_file),
                            args->spawn,
                            args->period_ticket >= 0,
                            std::string(db_url));

            const unsigned num_threads = std::thread::hardware_concurrency();
            net::io_context ioc(num_threads);

            net::signal_set signals(ioc, SIGINT, SIGTERM);
            signals.async_wait([&ioc](const boost::system::error_code& ec, [[maybe_unused]] int) {
                if (!ec) ioc.stop();
            });

            auto api_strand = net::make_strand(ioc);
            auto handler = std::make_shared<http_handler::RequestHandler>(app, www_root, api_strand);

            auto ticker = std::make_shared<http_handler::Ticker>(api_strand, std::chrono::milliseconds(args->period_ticket),
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