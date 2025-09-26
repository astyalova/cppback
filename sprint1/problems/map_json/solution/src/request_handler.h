#pragma once
#include "http_server.h"
#include "model.h"
#include "json_serializer.h"

#include <boost/json.hpp>
#include <string>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const std::string target = std::string(req.target());

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "MyGameServer");
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());

        try {
            const std::string api_prefix   = "/api/v1/";
            const std::string maps_path    = "/api/v1/maps";
            const std::string maps_prefix  = "/api/v1/maps/";

            // uncorrect API version
            if (target.rfind(api_prefix, 0) != 0) {
                json::object body;
                body["code"] = "badRequest";
                body["message"] = "Invalid API version";
                res.result(http::status::bad_request);
                res.body() = json::serialize(body);
                res.prepare_payload();
                return send(std::move(res));
            }

            if (target == maps_path) {
                res.body() = json_serializer::SerializeMaps(game_.GetMaps());
                
            }
            // correct map
            else if (target.rfind(maps_prefix, 0) == 0) {
                const std::string map_id = target.substr(maps_prefix.size());
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
            // correct prefix but inknown resource -> 404
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
};

}  // namespace http_handler