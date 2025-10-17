#pragma once

#include "http_server.h"
#include "model.h"
#include "json_serializer.h"
#include "json_logger.h"
#include "player.h"

#include <boost/json.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <random>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace net = boost::asio;

namespace api {
    inline const std::string_view API_PREFIX = "/api/v1/";
    inline const std::string_view MAPS_PATH = "/api/v1/maps";
    inline const std::string_view MAPS_PREFIX = "/api/v1/maps/";
}

struct ContentType {
    constexpr static std::string_view TEXT_HTML = "text/html";
    constexpr static std::string_view TEXT_CSS = "text/css";
    constexpr static std::string_view TEXT_PLAIN = "text/plain";
    constexpr static std::string_view TEXT_JAVASCRIPT = "text/javascript";
    constexpr static std::string_view APPLICATION_JSON = "application/json";
    constexpr static std::string_view APPLICATION_XML = "application/xml";
    constexpr static std::string_view APPLICATION_OCTET_STREAM = "application/octet-stream";
    constexpr static std::string_view IMAGE_PNG = "image/png";
    constexpr static std::string_view IMAGE_JPEG = "image/jpeg";
    constexpr static std::string_view IMAGE_GIF = "image/gif";
    constexpr static std::string_view IMAGE_BMP = "image/bmp";
    constexpr static std::string_view IMAGE_MICROSOFT_ICON = "image/vnd.microsoft.icon";
    constexpr static std::string_view IMAGE_TIFF = "image/tiff";
    constexpr static std::string_view IMAGE_SVG_XML = "image/svg+xml";
    constexpr static std::string_view AUDIO_MPEG = "audio/mpeg";
    constexpr static std::string_view UNKNOWN = "";

    static std::string_view GetContentTypeByFileExtension(fs::path file_path) {
        static const std::unordered_map<std::string, std::string_view> types = {
            {".htm", TEXT_HTML}, {".html", TEXT_HTML},
            {".css", TEXT_CSS}, {".txt", TEXT_PLAIN},
            {".js", TEXT_JAVASCRIPT}, {".json", APPLICATION_JSON},
            {".xml", APPLICATION_XML}, {".png", IMAGE_PNG},
            {".jpg", IMAGE_JPEG}, {".jpeg", IMAGE_JPEG},
            {".jpe", IMAGE_JPEG}, {".gif", IMAGE_GIF},
            {".bmp", IMAGE_BMP}, {".ico", IMAGE_MICROSOFT_ICON},
            {".tiff", IMAGE_TIFF}, {".tif", IMAGE_TIFF},
            {".svg", IMAGE_SVG_XML}, {".svgz", IMAGE_SVG_XML},
            {".mp3", AUDIO_MPEG}
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

    explicit RequestHandler(model::Game& game, player::Players& players, const std::string &data_path, Strand api_strand) :
          game_{game},
          players_{players},
          data_path_{fs::weakly_canonical(data_path)},
          api_strand_{std::move(api_strand)} {}

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

        if (!obj["userName"].is_string() || !obj["mapId"].is_string()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Bad field types"));
            return;
        }

        std::string user_name = Trim(obj["userName"].as_string());
        std::string map_id = Trim(obj["mapId"].as_string());

        if (user_name.empty()) {
            send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Empty userName"));
            return;
        }

        const auto* map = game_.FindMap(model::Map::Id{map_id});
        if (!map) {
            send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found"));
            return;
        }

        model::GameSession* session = game_.FindSession(map);
        if (!session) {
            session = game_.CreateSession(map);
        }

        model::Dog* dog = session->CreateDog(user_name);
        auto [player, token] = players_.Add(dog, session);

        boost::json::object result;
        result["authToken"] = token;
        result["playerId"] = player->GetDogId();

        http::response<http::string_body> res(http::status::ok, req.version());
        res.set(http::field::server, "MyGameServer");
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");
        res.body() = boost::json::serialize(result);
        res.prepare_payload();

        send(std::move(res));
    }

void HandleApiPlayers(http::request<http::string_body>&& req,
                      std::function<void(http::response<http::string_body>)> send) {
    auto auth_it = req.find(http::field::authorization);
    if (auth_it == req.end()) {
        send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Missing token"));
        return;
    }

    std::string auth_header = std::string(auth_it->value());
    const std::string bearer_prefix = "Bearer ";
    if (auth_header.size() < bearer_prefix.size() || auth_header.substr(0, bearer_prefix.size()) != bearer_prefix) {
        send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Missing or invalid Bearer token"));
        return;
    }

    std::string token = auth_header.substr(bearer_prefix.size());
    if (token.empty()) {
        send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Empty token"));
        return;
    }

    auto player = players_.FindByToken(token);
    if (!player) {
        send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Unknown token"));
        return;
    }

    auto session = player->GetSession();
    auto session_dogs = session->GetDogs();

    boost::json::array arr;
    for (auto* dog : session_dogs) {
        boost::json::object obj;
        obj["name"] = dog->GetNickname();
        obj["id"] = dog->GetToken();
        arr.push_back(std::move(obj));
    }

    boost::json::object result;
    result["players"] = std::move(arr);

    http::response<http::string_body> res(http::status::ok, req.version());
    res.set(http::field::server, "MyGameServer");
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");

    if (req.method() == http::verb::head) {
        res.body().clear();
    } else {
        res.body() = boost::json::serialize(result);
    }

    res.prepare_payload();
    send(std::move(res));
}



    void HandleApiRequest(http::request<http::string_body>&& req, std::function<void(http::response<http::string_body>)> send) {
        const std::string target = std::string(req.target());
        const auto method = req.method();

        if (target == "/api/v1/game/join") {
            if (method == http::verb::post) {
                net::dispatch(api_strand_,
                    [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                        self->HandleApiJoin(std::move(req), std::move(send));
                    });
                return;
            } else {
                auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Method not allowed");
                res.set(http::field::allow, "POST");
                send(std::move(res));
                return;
            }
        }

        if (target == "/api/v1/game/players") {
            if (method == http::verb::get || method == http::verb::head) {
                net::dispatch(api_strand_,
                    [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                        self->HandleApiPlayers(std::move(req), std::move(send));
                    });
                return;
            } else {
                auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Method not allowed");
                res.set(http::field::allow, "GET, HEAD");
                send(std::move(res));
                return;
            }
        }

        if (target == api::MAPS_PATH) {
            if (method == http::verb::get) {
                net::dispatch(api_strand_,
                    [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                        http::response<http::string_body> res{http::status::ok, req.version()};
                        res.set(http::field::server, "MyGameServer");
                        res.set(http::field::content_type, ContentType::APPLICATION_JSON);
                        res.set(http::field::cache_control, "no-cache");
                        res.keep_alive(req.keep_alive());
                        res.body() = json_serializer::SerializeMaps(self->game_.GetMaps());
                        res.prepare_payload();
                        send(std::move(res));
                    });
                return;
            } else {
                auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Method not allowed");
                res.set(http::field::allow, "GET");
                send(std::move(res));
                return;
            }
        }

        if (target.rfind(api::MAPS_PREFIX, 0) == 0) {
            if (method == http::verb::get) {
                const std::string map_id = target.substr(api::MAPS_PREFIX.size());
                net::dispatch(api_strand_,
                    [self = shared_from_this(), req = std::move(req), send = std::move(send), map_id]() mutable {
                        http::response<http::string_body> res{http::status::ok, req.version()};
                        res.set(http::field::server, "MyGameServer");
                        res.set(http::field::content_type, ContentType::APPLICATION_JSON);
                        res.set(http::field::cache_control, "no-cache");
                        res.keep_alive(req.keep_alive());

                        const auto* map = self->game_.FindMap(model::Map::Id(map_id));
                        if (map) {
                            res.body() = json_serializer::SerializeMap(*map);
                        } else {
                            boost::json::object body;
                            body["code"] = "mapNotFound";
                            body["message"] = "Map not found";
                            res.result(http::status::not_found);
                            res.body() = boost::json::serialize(body);
                        }
                        res.prepare_payload();
                        send(std::move(res));
                    });
                return;
            } else {
                auto res = MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Method not allowed");
                res.set(http::field::allow, "GET");
                send(std::move(res));
                return;
            }
        }

        net::dispatch(api_strand_, [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {
                send(MakeErrorResponse(http::status::not_found, "notFound", "Unknown endpoint"));
            });
    }

private:
    model::Game& game_;
    player::Players& players_;
    fs::path data_path_;
    Strand api_strand_;

    static std::string UrlDecode(std::string_view str) {
        std::string result;
        result.reserve(str.size());
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%' && i + 2 < str.size()) {
                int value = 0;
                std::istringstream iss(std::string(str.substr(i + 1, 2)));
                iss >> std::hex >> value;
                result.push_back(static_cast<char>(value));
                i += 2;
            } else if (str[i] == '+') {
                result.push_back(' ');
            } else {
                result.push_back(str[i]);
            }
        }
        return result;
    }

    static std::string Trim(std::string_view sv) {
        size_t b = 0;
        while (b < sv.size() && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
        size_t e = sv.size();
        while (e > b && std::isspace(static_cast<unsigned char>(sv[e-1]))) --e;
        return std::string(sv.substr(b, e - b));
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleStatic(
        const http::request<Body, http::basic_fields<Allocator>>& req
    ) const {
        auto rel_path = UrlDecode(std::string(req.target()));
        if (rel_path.empty() || rel_path[0] != '/') {
            rel_path.insert(rel_path.begin(), '/');
        }

        fs::path requested = fs::weakly_canonical(data_path_ / rel_path.substr(1));

        if (requested.string().find(data_path_.string()) != 0) {
            http::response<http::string_body> res{http::status::bad_request, req.version()};
            res.set(http::field::content_type, ContentType::TEXT_PLAIN);
            res.body() = "Bad Request";
            res.prepare_payload();
            return res;
        }

        if (fs::is_directory(requested)) {
            requested /= "index.html";
        }

        if (!fs::exists(requested) || !fs::is_regular_file(requested)) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, ContentType::TEXT_PLAIN);
            res.body() = "Not Found";
            res.prepare_payload();
            return res;
        }

        const auto file_size = fs::file_size(requested);
        const auto mime = ContentType::GetContentTypeByFileExtension(requested);

        if (req.method() == http::verb::head) {
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, mime);
            res.content_length(file_size);
            res.keep_alive(req.keep_alive());
            return res;
        }

        std::ifstream file(requested, std::ios::binary);
        std::ostringstream buffer;
        buffer << file.rdbuf();

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, mime);
        res.content_length(file_size);
        res.body() = buffer.str();
        res.keep_alive(req.keep_alive());
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
