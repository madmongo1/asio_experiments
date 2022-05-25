// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

#include <asio/error.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_future.hpp>
#include <asioex/async.hpp>

#include <boost/scope_exit.hpp>

template<typename CompletionToken>
auto async_foo(asio::steady_timer &tim,
               CompletionToken && tk,
               asioex::compose_tag<void(std::error_code, int)> = {})
    -> typename asio::async_result<std::decay_t<CompletionToken>,
                                   void(std::error_code, int)>::return_type
{
    printf("Entered\n");
    auto [ec] = co_await tim.async_wait(asio::experimental::deferred);
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
    async_foo(tim,
              [&](std::error_code ec_, int s)
              {
                    res = s;
                    ec = ec_;
              });


    auto ff = async_foo(tim, asio::use_future);

    ctx.run();

    ff.get();
    CHECK(res == 42);
    CHECK(ec == asio::error::host_not_found_try_again);

}


TEST_SUITE_END();