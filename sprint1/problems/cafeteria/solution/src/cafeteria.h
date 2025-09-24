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
#include <utility>
#include <stdexcept>

#include "hotdog.h"
#include "result.h"
#include "ingredients.h"
#include "gascooker.h"

namespace net = boost::asio;

// Колбэк, который вызывается, когда хот-дог готов или произошла ошибка
using HotDogHandler = std::function<void(Result<HotDog>)>;

class Cafeteria : public std::enable_shared_from_this<Cafeteria> {
public:
    explicit Cafeteria(net::io_context& io)
        : io_(io)
        , strand_(net::make_strand(io_))
        , gas_cooker_(std::make_shared<GasCooker>(io_))
    {}

    // Асинхронно готовит хот-дог
    void OrderHotDog(HotDogHandler handler) {
        net::dispatch(
            strand_,
            [self = shared_from_this(), handler = std::move(handler)]() mutable {
                self->StartCooking(std::move(handler));
            });
    }

private:
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    std::shared_ptr<GasCooker> gas_cooker_;
    Store store_;

    // Берём «среднее» время готовки, чтобы попасть в допустимый интервал
    static Clock::duration SausageTargetDuration() {
        return (HotDog::MIN_SAUSAGE_COOK_DURATION + HotDog::MAX_SAUSAGE_COOK_DURATION) / 2;
    }
    static Clock::duration BreadTargetDuration() {
        return (HotDog::MIN_BREAD_COOK_DURATION + HotDog::MAX_BREAD_COOK_DURATION) / 2;
    }

    void StartCooking(HotDogHandler handler) {
        auto sausage = store_.GetSausage();
        auto bread   = store_.GetBread();
        const int id = store_.NextOrderId();

        // Начинаем жарить сосиску
        sausage->StartFry(*gas_cooker_,
            [self = shared_from_this(),
             sausage,
             bread,
             id,
             handler]() mutable {
                auto timer = std::make_shared<net::steady_timer>(
                    self->io_, SausageTargetDuration());

                timer->async_wait(
                    net::bind_executor(
                        self->strand_,
                        [self, sausage, bread, id, handler, timer](const boost::system::error_code& ec) mutable {
                            if (ec) {
                                handler(Result<HotDog>{std::make_exception_ptr(std::runtime_error(ec.message()))});
                                return;
                            }

                            try {
                                sausage->StopFry();
                            } catch (...) {
                                handler(Result<HotDog>{std::current_exception()});
                                return;
                            }

                            self->StartBreadBake(std::move(sausage), std::move(bread), id, std::move(handler));
                        }));
            });
    }

    void StartBreadBake(std::shared_ptr<Sausage> sausage,
                        std::shared_ptr<Bread> bread,
                        int id,
                        HotDogHandler handler) {
        bread->StartBake(*gas_cooker_,
            [self = shared_from_this(), sausage, bread, id, handler]() mutable {
                auto timer = std::make_shared<net::steady_timer>(
                    self->io_, BreadTargetDuration());

                timer->async_wait(
                    net::bind_executor(
                        self->strand_,
                        [sausage, bread, id, handler, timer](const boost::system::error_code& ec) mutable {
                            if (ec) {
                                handler(Result<HotDog>{std::make_exception_ptr(std::runtime_error(ec.message()))});
                                return;
                            }

                            try {
                                bread->StopBaking();
                            } catch (...) {
                                handler(Result<HotDog>{std::current_exception()});
                                return;
                            }

                            try {
                                HotDog hotdog(id, sausage, bread);
                                handler(Result<HotDog>{std::move(hotdog)});
                            } catch (...) {
                                handler(Result<HotDog>{std::current_exception()});
                            }
                        }));
            });
    }
};
