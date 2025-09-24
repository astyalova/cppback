#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"
#include "store.h"      
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
        // захватываем shared_ptr чтобы объект жил, пока идёт готовка
        net::dispatch(strand_, [self = shared_from_this(), handler = std::move(handler)] {
            self->StartCooking(std::move(handler));
        });
    }

private:
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    std::shared_ptr<GasCooker> gas_cooker_;
    Store store_; 

    void StartCooking(HotDogHandler handler) {
        // 1. Получаем сырые ингредиенты
        auto sausage = store_.TakeSausage();
        auto bread   = store_.TakeBread();
        const int id = store_.NextOrderId();

        // 2. Занимаем 2 горелки: для сосиски и для булки
        gas_cooker_->UseBurner([self = shared_from_this(),
                                sausage,
                                bread,
                                id,
                                handler]() mutable {
            self->CookSausage(std::move(sausage), std::move(bread), id, std::move(handler));
        });
    }

    void CookSausage(std::shared_ptr<Sausage> sausage,
                     std::shared_ptr<Bread> bread,
                     int id,
                     HotDogHandler handler) {
        // время готовки сосиски
        auto timer = std::make_shared<net::steady_timer>(
            io_, sausage->RequiredCookTime());

        timer->async_wait(net::bind_executor(
            strand_,
            [self = shared_from_this(),
             sausage,
             bread,
             id,
             handler,
             timer](const boost::system::error_code& ec) mutable {
                if (ec) {
                    handler(Result<HotDog>::Fail(ec.message()));
                    return;
                }
                sausage->FinishCook();
                // после сосиски готовим хлеб
                self->CookBread(std::move(sausage), std::move(bread), id, std::move(handler));
            }));
    }

    void CookBread(std::shared_ptr<Sausage> sausage,
                   std::shared_ptr<Bread> bread,
                   int id,
                   HotDogHandler handler) {
        auto timer = std::make_shared<net::steady_timer>(
            io_, bread->RequiredBakeTime());

        timer->async_wait(net::bind_executor(
            strand_,
            [sausage,
             bread,
             id,
             handler,
             timer](const boost::system::error_code& ec) mutable {
                if (ec) {
                    handler(Result<HotDog>::Fail(ec.message()));
                    return;
                }
                bread->FinishBake();

                try {
                    HotDog hotdog(id, sausage, bread);
                    handler(Result<HotDog>::Ok(std::move(hotdog)));
                } catch (const std::exception& e) {
                    handler(Result<HotDog>::Fail(e.what()));
                }
            }));
    }
};
