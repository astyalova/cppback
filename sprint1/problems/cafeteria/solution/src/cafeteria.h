#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>

#include <memory>
#include <chrono>
#include <stdexcept>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

using HotDogHandler = std::function<void(Result<HotDog>)>;

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_(io)
        , strand_(net::make_strand(io_))
        , gas_cooker_(std::make_shared<GasCooker>(io_))
    {}

    void OrderHotDog(HotDogHandler handler) {
        net::dispatch(strand_, [this, handler = std::move(handler)]() mutable {
            StartCooking(std::move(handler));
        });
    }

private:
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    std::shared_ptr<GasCooker> gas_cooker_;
    Store store_;

    static Clock::duration SausageTargetDuration() {
        return (HotDog::MIN_SAUSAGE_COOK_DURATION + HotDog::MAX_SAUSAGE_COOK_DURATION) / 2;
    }
    static Clock::duration BreadTargetDuration() {
        return (HotDog::MIN_BREAD_COOK_DURATION + HotDog::MAX_BREAD_COOK_DURATION) / 2;
    }

    void StartCooking(HotDogHandler handler) {
        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();
        int id = store_.NextOrderId();

        auto sausage_timer = std::make_shared<net::steady_timer>(io_, SausageTargetDuration());
        auto bread_timer = std::make_shared<net::steady_timer>(io_, BreadTargetDuration());

        sausage->StartFry(*gas_cooker_, [sausage_timer, this, sausage, bread, id, handler = std::move(handler), bread_timer]() mutable {
            sausage_timer->async_wait(net::bind_executor(strand_, [this, sausage, bread, id, handler = std::move(handler), bread_timer](const boost::system::error_code& ec) mutable {
                if (ec) {
                    handler(Result<HotDog>{std::make_exception_ptr(std::runtime_error(ec.message()))});
                    return;
                }

                try { sausage->StopFry(); }
                catch (...) { handler(Result<HotDog>{std::current_exception()}); return; }

                bread->StartBake(*gas_cooker_, [bread_timer, this, sausage, bread, id, handler = std::move(handler)]() mutable {
                    bread_timer->async_wait(net::bind_executor(strand_, [sausage, bread, id, handler = std::move(handler)](const boost::system::error_code& ec) mutable {
                        if (ec) {
                            handler(Result<HotDog>{std::make_exception_ptr(std::runtime_error(ec.message()))});
                            return;
                        }

                        try { bread->StopBaking(); }
                        catch (...) { handler(Result<HotDog>{std::current_exception()}); return; }

                        try { handler(Result<HotDog>{HotDog(id, sausage, bread)}); }
                        catch (...) { handler(Result<HotDog>{std::current_exception()}); }
                    }));
                });
            }));
        });
    }
};
