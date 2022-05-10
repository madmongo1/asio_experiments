// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <asioex/redirect_cancellation.hpp>
#include <asio.hpp>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

asio::awaitable<void> redirect_cancellation()
{
    asio::error_code res;
    asio::cancellation_type cnc;
    asio::cancellation_signal sig;
    bool done;
    asio::steady_timer tim{co_await asio::this_coro::executor, std::chrono::steady_clock::time_point::max()};

    tim.async_wait(
            asioex::redirect_cancellation(
                asio::bind_cancellation_slot(
                    sig.slot(),
                [&](asio::error_code ec)
                {
                    done = true;
                    res = ec;
                }), cnc));

    co_await asio::post(asio::use_awaitable);
    sig.emit(asio::cancellation_type::partial);
    co_await asio::post(asio::use_awaitable);

    CHECK(res == asio::error_code{});
    CHECK(cnc == asio::cancellation_type::partial);
    CHECK(done);
    co_return ;
}

TEST_CASE("redirect_cancellation")
{
    asio::io_context ctx;
    asio::co_spawn(ctx, redirect_cancellation(), asio::detached);
    ctx.run();
}