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

#include "hotdog.h"
#include "result.h"
#include "ingredients.h"
#include "gascooker.h"

namespace net = boost::asio;

// Колбэк, который вызывается когда хот-дог готов или произошла ошибка
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
        // Запускаем работу через strand, чтобы все операции шли последовательно для данного объекта
        net::dispatch(strand_, [self = shared_from_this(), handler = std::move(handler)]() mutable {
            self->StartCooking(std::move(handler));
        });
    }

private:
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    std::shared_ptr<GasCooker> gas_cooker_;
    Store store_;

    // Выбираем безопасную длительность готовки: среднее между MIN и MAX,
    // чтобы гарантированно попасть в допустимый интервал
    static Clock::duration SausageTargetDuration() {
        using D = Clock::duration;
        return (HotDog::MIN_SAUSAGE_COOK_DURATION + HotDog::MAX_SAUSAGE_COOK_DURATION) / 2;
    }
    static Clock::duration BreadTargetDuration() {
        using D = Clock::duration;
        return (HotDog::MIN_BREAD_COOK_DURATION + HotDog::MAX_BREAD_COOK_DURATION) / 2;
    }

    void StartCooking(HotDogHandler handler) {
        auto sausage = store_.GetSausage();
        auto bread   = store_.GetBread();
        const int id = store_.NextOrderId();

        // Sausage::StartFry вызывает handler, когда горелка занята и обжаривание начинается.
        // В этом handler мы ставим таймер на целевую длительность и по его истечении завершаем жарку.
        sausage->StartFry(*gas_cooker_, 
            [self = shared_from_this(),
             sausage,
             bread,
             id,
             handler]() mutable {
                // когда обжаривание реально началось — ставим таймер на нужную длительность
                auto timer = std::make_shared<net::steady_timer>(
                    self->io_, SausageTargetDuration());

                timer->async_wait(
                    boost::asio::bind_executor(
                        self->strand_,
                        [self, sausage, bread, id, handler, timer](const boost::system::error_code& ec) mutable {
                            if (ec) {
                                // Ошибка ожидания таймера
                                handler(Result<HotDog>::Error(ec.message()));
                                return;
                            }

                            // Завершаем обжаривание сосиски
                            try {
                                sausage->StopFry();
                            } catch (const std::exception& e) {
                                handler(Result<HotDog>::Error(e.what()));
                                return;
                            }

                            // После успешной готовки сосиски — начинаем выпечку хлеба
                            self->StartBreadBake(std::move(sausage), std::move(bread), id, std::move(handler));
                        }
                    )
                );
            }
        );
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
                    boost::asio::bind_executor(
                        self->strand_,
                        [self, sausage, bread, id, handler, timer](const boost::system::error_code& ec) mutable {
                            if (ec) {
                                handler(Result<HotDog>::Error(ec.message()));
                                return;
                            }

                            try {
                                bread->StopBaking();
                            } catch (const std::exception& e) {
                                handler(Result<HotDog>::Error(e.what()));
                                return;
                            }

                            // Собираем хот-дог и возвращаем результат
                            try {
                                HotDog hotdog(id, sausage, bread);
                                handler(Result<HotDog>::Success(std::move(hotdog)));
                            } catch (const std::exception& e) {
                                handler(Result<HotDog>::Error(e.what()));
                            }
                        }
                    )
                );
            }
        );
    }
};
