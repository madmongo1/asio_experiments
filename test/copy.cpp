// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <asio.hpp>
#include <asio/experimental/channel.hpp>
#include <asio/experimental/deferred.hpp>
#include <asioex/copy.hpp>

#include <iostream>

#define check_eq(x, y) \
    if (x != y) \
    {                  \
        std::cerr << __FILE__ "(" << __LINE__ << ") '" #x " != " #y "' failed: " << x << " != " << y << std::endl;               \
        errors ++; \
    }                  \


asio::awaitable<int> main_impl()
{
    int errors = 0;
    auto exec = co_await asio::this_coro::executor;
    asio::experimental::channel<void(std::error_code, int, double)> chan{exec, 4};

    std::vector<int> res_i;
    std::vector<double> res_d;

    auto source = chan.async_receive(asio::experimental::deferred);
    auto sink = [&](int i, double d, auto && token) {
        return asio::post(exec, asio::experimental::deferred([&, i, d]{
           res_i.push_back(i);
           res_d.push_back(i);
           return asio::experimental::deferred.values(asio::error_code{});
        }))(std::move(token));
    };

    co_await chan.async_send(asio::error_code{}, 1, 0.41, asio::use_awaitable);
    co_await chan.async_send(asio::error_code{}, 2, 0.42, asio::use_awaitable);
    co_await chan.async_send(asio::error_code{}, 3, 0.43, asio::use_awaitable);
    co_await chan.async_send(asio::error_code{}, 4, 0.44, asio::use_awaitable);

    asio::steady_timer  tim{exec, std::chrono::milliseconds(100)};
    tim.async_wait([&](auto) {chan.cancel(); chan.close();});

    asio::error_code ec;
    auto n = co_await asioex::async_copy(source, sink, asio::redirect_error(asio::use_awaitable, ec));

    check_eq(ec , asio::experimental::channel_errc::channel_cancelled);
    check_eq(n , 4);
    check_eq(res_i.size(), 4u);
    check_eq(res_d.size(), 4u);

    co_return errors;
}

int
main()
{
    asio::io_context ctx;

    int res = 0;

    asio::co_spawn(ctx, main_impl(),
                   [&](std::exception_ptr e, int r)
                   {
                       if (e)
                           std::rethrow_exception(e);
                       res = r;
                   });

    ctx.run();

    return 0;
}