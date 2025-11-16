#pragma once

#include "http_server.h"
#include "application.h"
#include "json_serializer.h"
#include "json_logger.h"

#include <boost/json.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <regex>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace net = boost::asio;

struct ContentType {
    constexpr static std::string_view TEXT_HTML = "text/html";
    constexpr static std::string_view TEXT_CSS = "text/css";
    constexpr static std::string_view TEXT_PLAIN = "text/plain";
    constexpr static std::string_view TEXT_JAVASCRIPT = "text/javascript";
    constexpr static std::string_view APPLICATION_JSON = "application/json";
    constexpr static std::string_view APPLICATION_OCTET_STREAM = "application/octet-stream";

    static std::string_view GetContentTypeByFileExtension(fs::path file_path) {
        static const std::unordered_map<std::string, std::string_view> types = {
            {".htm", TEXT_HTML}, {".html", TEXT_HTML},
            {".css", TEXT_CSS}, {".txt", TEXT_PLAIN},
            {".js", TEXT_JAVASCRIPT}, {".json", APPLICATION_JSON},
            {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
            {".gif", "image/gif"}, {".bmp", "image/bmp"}
        };

        std::string ext = file_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
        if (auto it = types.find(ext); it != types.end()) {
            return it->second;
        }
        return APPLICATION_OCTET_STREAM;
    }
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit RequestHandler(Application& app, const std::string& data_path, Strand api_strand)
        : app_{app}, data_path_{fs::weakly_canonical(data_path)}, api_strand_{api_strand} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const std::string target = std::string(req.target());

        if (target.rfind("/api/", 0) != 0) {
            return send(HandleStatic(req));
        }

        HandleApiRequest(http::request<http::string_body>(std::move(req)),
            [send = std::forward<Send>(send)](http::response<http::string_body>&& res) mutable {
                send(std::move(res));
            }
        );
    }

private:
    Application& app_;
    fs::path data_path_;
    Strand api_strand_;

    void HandleApiJoin(http::request<http::string_body>&& req, std::function<void(http::response<http::string_body>)> send) {
        boost::system::error_code ec;
        auto body = boost::json::parse(req.body(), ec);

        if (ec || !body.is_object()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid JSON"));
            return;
        }

        auto obj = body.as_object();
        if (!obj.contains("userName") || !obj.contains("mapId")) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Missing fields"));
            return;
        }

        std::string user_name = Trim(obj["userName"].as_string());
        std::string map_id = Trim(obj["mapId"].as_string());

        try {
            auto result = app_.JoinGame(user_name, map_id);

            http::response<http::string_body> res(http::status::ok, req.version());
            res.set(http::field::server, "MyGameServer");
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.body() = boost::json::serialize(result);
            res.prepare_payload();
            send(std::move(res));
        } catch (const AppErrorException& e) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", e.what()));
        }
    }

    void HandleApiPlayers(http::request<http::string_body>&& req, std::function<void(http::response<http::string_body>)> send) {
        auto token = ExtractToken(req);
        if (!token.has_value()) {
            send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Missing or invalid token"));
            return;
        }

        try {
            auto players_json = app_.GetPlayers(token.value());

            http::response<http::string_body> res(http::status::ok, req.version());
            res.set(http::field::server, "MyGameServer");
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            if (req.method() != http::verb::head) {
                res.body() = boost::json::serialize(players_json);
            }
            res.prepare_payload();
            send(std::move(res));
        } catch (const AppErrorException&) {
            send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Unknown token"));
        }
    }

    void HandleApiGameState(http::request<http::string_body>&& req, std::function<void(http::response<http::string_body>)> send) {
        auto token = ExtractToken(req);
        if (!token.has_value()) {
            send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Missing or invalid token"));
            return;
        }

        try {
            auto state_json = app_.GetGameState(token.value());
            auto state_obj = state_json.as_object();

            boost::json::object res_body;
            res_body["players"] = state_obj["players"];

            boost::json::object lost_objects_json;
            auto lost_objects = app_.GetLostObjects(token.value());

            for (const auto& [id, obj] : lost_objects) {
                boost::json::array pos{ static_cast<double>(obj.pos.x), static_cast<double>(obj.pos.y) };
                lost_objects_json[std::to_string(id)] = {
                    {"type", obj.type},
                    {"pos", pos}
                };
            }

            res_body["lostObjects"] = lost_objects_json;

            http::response<http::string_body> res(http::status::ok, req.version());
            res.set(http::field::server, "MyGameServer");
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            if (req.method() != http::verb::head) {
                res.body() = boost::json::serialize(res_body);
            }
            res.prepare_payload();
            send(std::move(res));
        } catch (const AppErrorException&) {
            send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Unknown token"));
        }
    }

    void HandleApiAction(http::request<http::string_body>&& req, std::function<void(http::response<http::string_body>)> send) {
        auto token = ExtractToken(req);
        if (!token.has_value()) {
            send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Missing or invalid token"));
            return;
        }

        boost::system::error_code ec;
        auto body = boost::json::parse(req.body(), ec);
        if (ec || !body.is_object()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action"));
            return;
        }

        auto obj = body.as_object();
        if (!obj.contains("move") || !obj["move"].is_string()) {
            send(MakeErrorResponse(http::status::bad_request,"invalidArgument", "Field 'move' is required"));
            return;
        }

        std::string move = obj["move"].as_string().c_str();
        static const std::unordered_set<std::string> valid_moves = {"L", "R", "U", "D", ""};
        if (valid_moves.find(move) == valid_moves.end()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid move value"));
            return;
        }

        try {
            app_.ActionPlayer(token.value(), move);
            http::response<http::string_body> res(http::status::ok, req.version());
            res.set(http::field::server, "MyGameServer");
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.body() = "{}";
            res.prepare_payload();
            send(std::move(res));
        } catch (const AppErrorException& e) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", e.what()));
        }
    }

    void HandleApiTick(http::request<http::string_body>&& req, std::function<void(http::response<http::string_body>)> send) {
        boost::system::error_code ec;
        auto body = boost::json::parse(req.body(), ec);
        if (ec || !body.is_object()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON"));
            return;
        }

        auto obj = body.as_object();
        if (!obj.contains("timeDelta") || !obj["timeDelta"].is_int64()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Missing timeDelta"));
            return;
        }

        int64_t time_delta = obj["timeDelta"].as_int64();
        try {
            app_.Tick(std::chrono::milliseconds(time_delta));
            http::response<http::string_body> res(http::status::ok, req.version());
            res.set(http::field::server, "MyGameServer");
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.body() = "{}";
            res.prepare_payload();
            send(std::move(res));
        } catch (const AppErrorException& e) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", e.what()));
        }
    }

    void HandleApiMapInfo(http::request<http::string_body>&& req, std::function<void(http::response<http::string_body>)> send) {
        std::string target = std::string(req.target());
        static const std::regex map_regex(R"(^/api/v1/maps/([^/]+)$)");
        std::smatch match;
        if (!std::regex_match(target, match, map_regex)) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid map id"));
            return;
        }

        std::string map_id = match[1].str();

        try {
            std::string map_json_str = app_.GetMapInfo(map_id);
            boost::json::value map_json = boost::json::parse(map_json_str);
            auto map_obj = map_json.as_object();

            boost::json::array lost_objects_array;
            auto map_lost_info = app_.GetMapLostObjectsInfo(map_id);

            for (int i = 0; i < map_lost_info.loot_type_count; ++i) {
                lost_objects_array.push_back(boost::json::object{{"type", i}});
            }
            map_obj["lostObjects"] = lost_objects_array;

            http::response<http::string_body> res(http::status::ok, req.version());
            res.set(http::field::server, "MyGameServer");
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            if (req.method() != http::verb::head) {
                res.body() = boost::json::serialize(map_obj);
            }
            res.prepare_payload();
            send(std::move(res));
        } catch (const AppErrorException& e) {
            send(MakeErrorResponse(http::status::not_found, "mapNotFound", e.what()));
        }
    }


    static std::string Trim(std::string_view sv) {
        size_t b = 0, e = sv.size();
        while (b < e && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
        while (e > b && std::isspace(static_cast<unsigned char>(sv[e-1]))) --e;
        return std::string(sv.substr(b, e - b));
    }

    std::optional<std::string> ExtractToken(const http::request<http::string_body>& req) const {
        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) return std::nullopt;

        std::string auth_header = std::string(auth_it->value());
        static const std::regex token_regex(R"(Bearer\s([0-9a-fA-F]{32}))");
        std::smatch match_results;
        if (!std::regex_match(auth_header, match_results, token_regex)) return std::nullopt;

        return match_results[1].str();
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleStatic(
        const http::request<Body, http::basic_fields<Allocator>>& req
    ) const {
        auto rel_path = std::string(req.target());
        if (rel_path.empty() || rel_path[0] != '/') rel_path.insert(rel_path.begin(), '/');

        fs::path requested = fs::weakly_canonical(data_path_ / rel_path.substr(1));
        if (requested.string().find(data_path_.string()) != 0) {
            http::response<http::string_body> res{http::status::bad_request, req.version()};
            res.set(http::field::content_type, ContentType::TEXT_PLAIN);
            res.body() = "Bad Request";
            res.prepare_payload();
            return res;
        }

        if (fs::is_directory(requested)) requested /= "index.html";
        if (!fs::exists(requested) || !fs::is_regular_file(requested)) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, ContentType::TEXT_PLAIN);
            res.body() = "Not Found";
            res.prepare_payload();
            return res;
        }

        std::ifstream file(requested, std::ios::binary);
        std::ostringstream buffer;
        buffer << file.rdbuf();

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, ContentType::GetContentTypeByFileExtension(requested));
        res.body() = buffer.str();
        res.prepare_payload();
        return res;
    }

    static http::response<http::string_body> MakeErrorResponse(
        http::status status, std::string_view code, std::string_view message) {
        boost::json::object body;
        body["code"] = code;
        body["message"] = message;

        http::response<http::string_body> res(status, 11);
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");
        res.body() = boost::json::serialize(body);
        res.prepare_payload();
        return res;
    }

    static http::response<http::string_body> MakeMethodNotAllowed(std::string_view message, std::string_view allow = "GET, HEAD") {
        boost::json::object body;
        body["code"] = "invalidMethod";
        body["message"] = message;
        

        http::response<http::string_body> res(http::status::method_not_allowed, 11);
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");
        res.set(http::field::allow, allow);
        res.body() = boost::json::serialize(body);
        res.prepare_payload();
        return res;
    }

    void HandleApiRequest(http::request<http::string_body>&& req, std::function<void(http::response<http::string_body>)> send) {
        const std::string target = std::string(req.target());
        const auto method = req.method();

        if (target == "/api/v1/game/join" && method == http::verb::post) {
            net::dispatch(api_strand_, [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                self->HandleApiJoin(std::move(req), std::move(send));
            });
            return;
        }

        if (target == "/api/v1/game/player/action" && method == http::verb::post) {
            net::dispatch(api_strand_, [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                self->HandleApiAction(std::move(req), std::move(send));
            });
            return;
        }

        if (target == "/api/v1/game/players" && (method == http::verb::get || method == http::verb::head)) {
            net::dispatch(api_strand_, [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                self->HandleApiPlayers(std::move(req), std::move(send));
            });
            return;
        }

        if(target == "/api/v1/game/state") {
            if (method != http::verb::get && method != http::verb::head) {
                send(MakeMethodNotAllowed("Only GET/HEAD methods are allowed for this endpoint", "GET, HEAD"));
                return;
            }
            net::dispatch(api_strand_, [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                self->HandleApiGameState(std::move(req), std::move(send));
            });
            return;
        }

        if (target == "/api/v1/game/tick") {
            if (method == http::verb::post) {
                net::dispatch(api_strand_, [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                    self->HandleApiTick(std::move(req), std::move(send));
                });
                return;
            } else {
                send(MakeMethodNotAllowed("Only POST method is allowed for this endpoint"));
                return;
            }
        }

        if (std::regex_match(target, std::regex(R"(^/api/v1/maps/[^/]+$)"))) {
            if (method != http::verb::get && method != http::verb::head) {
                send(MakeMethodNotAllowed("Only GET/HEAD methods are allowed for this endpoint", "GET, HEAD"));
                return;
            }
            net::dispatch(api_strand_, [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                self->HandleApiMapInfo(std::move(req), std::move(send));
            });
            return;
        }


        send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Unknown API endpoint"));
    }
};

template <typename Handler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(Handler& decorated)
        : decorated_{decorated} {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>&& req,
                    Send&& send) {
        LogRequest(req);
    decorated_(std::move(req),
        [this, &send, start = std::chrono::steady_clock::now()](auto&& response) mutable {
            auto end = std::chrono::steady_clock::now();
            auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            LogResponse(response, response_time);  
            send(std::forward<decltype(response)>(response));
        });
    }

private:
    Handler& decorated_;

  template <typename Body, typename Allocator>
    void LogRequest(const boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>& req) {
        boost::json::object log_data;
        log_data["ip"] = "0.0.0.0";
        log_data["URI"] = std::string(req.target());
        log_data["method"] = std::string(req.method_string());

        json_logger::LogData("request received", log_data);
    }

    template <typename Body>
    void LogResponse(const boost::beast::http::response<Body>& res, std::uint64_t response_time_ms) {
        boost::json::object log_data;
        log_data["response_time"] = response_time_ms;
        log_data["code"] = res.result_int();
        log_data["content_type"] = res.base().count(boost::beast::http::field::content_type) ?
                                   std::string(res.base().at(boost::beast::http::field::content_type)) :
                                   nullptr;

        json_logger::LogData("response sent", log_data);
    }
};

}  // namespace http_handler
