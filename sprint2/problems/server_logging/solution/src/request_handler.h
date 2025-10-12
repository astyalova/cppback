#pragma once
#include "http_server.h"
#include "model.h"
#include "json_serializer.h"
#include "json_logger.h"

#include <boost/json.hpp>
#include <boost/beast/http.hpp>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace sys = boost::system;
namespace fs = std::filesystem;

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

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, const std::string& data_path)
        : game_{game},
          data_path_{fs::weakly_canonical(data_path)} {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const std::string target = std::string(req.target());

        if (target.rfind("/api/", 0) != 0) {
            return send(HandleStatic(req));
        }

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "MyGameServer");
        res.set(http::field::content_type, ContentType::APPLICATION_JSON);
        res.keep_alive(req.keep_alive());

        try {

            if (target.rfind(api::API_PREFIX, 0) != 0) {
                json::object body;
                body["code"] = "badRequest";
                body["message"] = "Invalid API version";
                res.result(http::status::bad_request);
                res.body() = json::serialize(body);
                res.prepare_payload();
                return send(std::move(res));
            }

            if (target == api::MAPS_PATH) {
                res.body() = json_serializer::SerializeMaps(game_.GetMaps());
            }
            else if (target.rfind(api::MAPS_PREFIX, 0) == 0) {
                const std::string map_id = target.substr(api::MAPS_PREFIX.size());
                const auto* map = game_.FindMap(model::Map::Id(map_id));
                if (map) {
                    res.body() = json_serializer::SerializeMap(*map);
                } else {
                    json::object body;
                    body["code"] = "mapNotFound";
                    body["message"] = "Map not found";
                    res.result(http::status::not_found);
                    res.body() = json::serialize(body);
                }
            }
            else {
                json::object body;
                body["code"] = "notFound";
                body["message"] = "Unknown endpoint";
                res.result(http::status::not_found);
                res.body() = json::serialize(body);
            }
        } catch (const std::exception& e) {
            res.result(http::status::internal_server_error);
            json::object body;
            body["code"] = "internalError";
            body["message"] = e.what();
            res.body() = json::serialize(body);
        }

        res.prepare_payload();
        send(std::move(res));
    }

private:
    model::Game& game_;
    fs::path data_path_;

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
                   [this, &send](auto&& response) {
                       LogResponse(response);
                       send(std::forward<decltype(response)>(response));
                   });
    }

private:
    Handler& decorated_;

    template <typename Body, typename Allocator>
    void LogRequest(const boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>& req) {
        boost::json::object log_data;
        log_data["method"] = std::string(req.method_string());
        log_data["URI"] = std::string(req.target());
        log_data["version"] = (int)req.version();

        json_logger::LogData("request received", log_data);
    }

    template <typename Body>
    void LogResponse(const boost::beast::http::response<Body>& res) {
        boost::json::object log_data;
        log_data["status"] = static_cast<unsigned>(res.result());
        log_data["reason"] = std::string(res.reason());
        log_data["content_length"] = res.body().size();

        json_logger::LogData("Outgoing response", log_data);
    }
};

}  // namespace http_handler
