// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

#include <asio/bind_allocator.hpp>
#include <asio/detached.hpp>
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

struct test_composed_op
{
    std::size_t idx = 0;

    template<typename Self>
    void operator()(Self && self)
    {
        if (idx > 0)
            assert(self.get_executor().running_in_this_thread());
        idx++;
        if (idx < 1000000u)
            asio::post(self.get_executor(), std::move(self));
        else
            self.complete({});
    }
};

template<typename CompletionToken>
void run_composed_op(asio::io_context & ctx, CompletionToken && token)
{
    asio::async_compose<CompletionToken, void(asio::error_code)>(
        test_composed_op{
        }, token, ctx);
}

template<typename CompletionToken>
auto async_benchmark(asio::io_context &ctx,
                     CompletionToken && tk_,
                     asioex::compose_tag<void(std::error_code)> = {})
    -> typename asio::async_result<std::decay_t<CompletionToken>,
                                                void(std::error_code)>::return_type
{
    for (std::size_t idx = 0u; idx < 1000000u; idx++)
    {
        assert(ctx.get_executor().running_in_this_thread());

        co_await asio::post(ctx.get_executor(), asioex::compose_token(tk_));
    }

    co_return {};
}



TEST_CASE("single op benchmark")
{
    using clock = std::chrono::steady_clock;

    SUBCASE("naked composed op")
    {
        asio::io_context ctx;
        run_composed_op(ctx, asio::detached);
        auto start = clock::now();
        ctx.run();
        auto end = clock::now();

        std::printf("Naked     composed op took %lldns\n", std::chrono::nanoseconds(end - start).count());
    }

    SUBCASE("recycling composed op")
    {
        asio::recycling_allocator<void> alloc;
        asio::io_context ctx;
        run_composed_op(ctx, asio::bind_allocator(alloc, asio::detached));
        auto start = clock::now();
        ctx.run();
        auto end = clock::now();

        std::printf("Recycling composed op took %lldns\n", std::chrono::nanoseconds(end - start).count());
    }

    SUBCASE("naked async op")
    {
        asio::io_context ctx;
        async_benchmark(ctx, asio::detached);
        auto start = clock::now();
        ctx.run();
        auto end = clock::now();

        std::printf("Naked         coro op took %lldns\n", std::chrono::nanoseconds(end - start).count());
    }

    SUBCASE("naked async op")
    {
        asio::recycling_allocator<void> alloc;
        asio::io_context ctx;
        async_benchmark(ctx, asio::bind_allocator(alloc, asio::detached));
        auto start = clock::now();
        ctx.run();
        auto end = clock::now();

        std::printf("Recycling     coro op took %lldns\n", std::chrono::nanoseconds(end - start).count());
    }
}




TEST_SUITE_END();