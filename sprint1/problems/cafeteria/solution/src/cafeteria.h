#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <functional>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

using HotDogHandler = std::function<void(Result<HotDog>)>;

class Cafeteria : public std::enable_shared_from_this<Cafeteria> {
public:
    explicit Cafeteria(net::io_context& io)
        : io_(io)
        , gas_cooker_(std::make_shared<GasCooker>(io_))
    {}

    void OrderHotDog(HotDogHandler handler) {
        auto order_id = store_.NextOrderId();
        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();

        struct Order : public std::enable_shared_from_this<Order> {
            Order(int id,
                  std::shared_ptr<Sausage> s,
                  std::shared_ptr<Bread> b,
                  std::shared_ptr<GasCooker> c,
                  net::io_context& io,
                  HotDogHandler h)
                : id_(id), sausage_(s), bun_(b), cooker_(c), io_(io), handler_(std::move(h)),
                  strand_(net::make_strand(io)),
                  sausage_timer_(io), bun_timer_(io) {}

            void Execute() {
                GrillSausage();
                BakeBun();
            }

        private:
            void GrillSausage() {
                sausage_->StartFry(*cooker_, net::bind_executor(strand_, [self = shared_from_this()]() {
                    self->sausage_timer_.expires_after(HotDog::MIN_SAUSAGE_COOK_DURATION);
                    self->sausage_timer_.async_wait([self](const boost::system::error_code& ec) {
                        self->OnGrilled(ec);
                    });
                }));
            }

            void OnGrilled(const boost::system::error_code& ec) {
                if (!ec) {
                    sausage_->StopFry();
                    is_grilled_ = true;
                    CheckReady(ec);
                } else {
                    handler_(Result<HotDog>{std::make_exception_ptr(std::runtime_error(ec.message()))});
                }
            }

            void BakeBun() {
                bun_->StartBake(*cooker_, net::bind_executor(strand_, [self = shared_from_this()]() {
                    self->bun_timer_.expires_after(HotDog::MIN_BREAD_COOK_DURATION);
                    self->bun_timer_.async_wait([self](const boost::system::error_code& ec) {
                        self->OnBaked(ec);
                    });
                }));
            }

            void OnBaked(const boost::system::error_code& ec) {
                if (!ec) {
                    bun_->StopBaking();
                    is_baked_ = true;
                    CheckReady(ec);
                } else {
                    handler_(Result<HotDog>{std::make_exception_ptr(std::runtime_error(ec.message()))});
                }
            }

            void CheckReady(const boost::system::error_code&) {
                if (is_grilled_ && is_baked_) {
                    handler_(Result<HotDog>{HotDog(id_, sausage_, bun_)});
                }
            }

            int id_;
            std::shared_ptr<Sausage> sausage_;
            std::shared_ptr<Bread> bun_;
            std::shared_ptr<GasCooker> cooker_;
            net::io_context& io_;
            net::strand<net::io_context::executor_type> strand_;
            net::steady_timer sausage_timer_;
            net::steady_timer bun_timer_;
            HotDogHandler handler_;
            bool is_grilled_ = false;
            bool is_baked_ = false;
        };

        std::make_shared<Order>(order_id, sausage, bread, gas_cooker_, io_, std::move(handler))->Execute();
    }

private:
    net::io_context& io_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_;
};
