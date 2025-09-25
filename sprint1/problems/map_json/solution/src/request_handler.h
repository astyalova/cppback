#pragma once
#include "http_server.h"
#include "model.h"
#include "json_serializer.h"

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        namespace http = boost::beast::http;
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "MyGameServer");
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());

        const std::string target = std::string(req.target());

        try {
            if (!target.starts_with("/api/v1/")) {
                boost::json::object body;
                body["code"] = "badRequest";
                body["message"] = "Invalid API version";
                res.result(http::status::bad_request);
                res.body() = boost::json::serialize(body);
            }
            else if (target == "/api/v1/maps") {
                res.body() = json_serializer::SerializeMaps(game_.GetMaps());
            }
            else if (target.starts_with("/api/v1/maps/")) {
                std::string map_id = target.substr(std::strlen("/api/v1/maps/"));
                if (const auto* map = game_.FindMap(model::Map::Id(map_id))) {
                    res.body() = json_serializer::SerializeMap(*map);
                } else {
                    boost::json::object body;
                    body["code"] = "mapNotFound";
                    body["message"] = "Map not found";
                    res.result(http::status::not_found);
                    res.body() = boost::json::serialize(body);
                }
            }
            else {
                boost::json::object body;
                body["code"] = "badRequest";
                body["message"] = "Unknown endpoint";
                res.result(http::status::bad_request);
                res.body() = boost::json::serialize(body);
            }
        }
        catch (const std::exception& e) {
            res.result(http::status::internal_server_error);
            boost::json::object body;
            body["code"] = "internalError";
            body["message"] = e.what();
            res.body() = boost::json::serialize(body);
        }

        res.prepare_payload();
        send(std::move(res));
    }


private:
    model::Game& game_;
};

}  // namespace http_handler
