// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

#include <asio/bind_allocator.hpp>
#include <asio/error.hpp>
#include <asio/recycling_allocator.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_future.hpp>
#include <asioex/async.hpp>

#include <boost/scope_exit.hpp>

template<typename CompletionToken>
auto async_wait(asio::steady_timer &tim,
                std::chrono::milliseconds ms,
                CompletionToken && tk_,
               asioex::compose_tag<void(std::error_code, int)> = {})
    -> typename asio::async_result<std::decay_t<CompletionToken>,
                                   void(std::error_code, int)>::return_type
{
    const auto tk = asioex::compose_token(tk_);
    tim.expires_after(ms);
    printf("Entered\n");
    auto [ec] = co_await tim.async_wait(tk);
    printf("Waited %s\n", ec.message().c_str());
    co_return {asio::error::host_not_found_try_again, 42};
}

TEST_SUITE_BEGIN("async");

TEST_CASE("basics")
{
    int res;
    std::error_code ec;

    asio::io_context ctx;
    asio::steady_timer tim{ctx};
    asio::recycling_allocator<void> alloc;
    async_wait(tim,
              std::chrono::milliseconds(10),
               asio::bind_allocator(
                   alloc,
                   [&](std::error_code ec_, int s)
                   {
                         res = s;
                         ec = ec_;
                   }));


    auto ff = async_wait(tim,
                        std::chrono::milliseconds(10),
                        asio::use_future);

    CHECK_NOTHROW(ctx.run());

    CHECK_THROWS(ff.get());
    CHECK(res == 42);
    CHECK(ec == asio::error::host_not_found_try_again);

}


TEST_SUITE_END();