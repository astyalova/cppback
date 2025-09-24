#pragma once
#include "http_server.h"
#include "model.h"

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
        const auto& target = req.target();
        
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "MyGameServer");
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());

        try {
            if (target == "/maps") {

                res.body() = SerializeMaps(game_.GetMaps());
            } else if (target.starts_with("/map/")) {
                std::string map_id = std::string(target.substr(5)); 
                const auto* map = game_.FindMap(model::Map::Id(map_id));
                if (map) {
                    res.body() = SerializeMap(*map);
                } else {
                    res.result(http::status::not_found);
                    res.body() = R"({"error":"Map not found"})";
                }
            } else {
                res.result(http::status::not_found);
                res.body() = R"({"error":"Unknown endpoint"})";
            }
        } catch (const std::exception& e) {
            res.result(http::status::internal_server_error);
            res.body() = std::string(R"({"error":")") + e.what() + "\"}";
        }

        res.prepare_payload();
        send(std::move(res));
    }

private:
    model::Game& game_;
};

}  // namespace http_handler
