// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

#include <asio/bind_allocator.hpp>
#include <asio/co_spawn.hpp>
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
    auto [ec] = co_await tim.async_wait(tk);
    co_return {asio::error::host_not_found_try_again, 42};
}


template<typename CompletionToken>
auto async_wait_none(asio::steady_timer &tim,
           std::chrono::milliseconds ms,
           CompletionToken && tk_,
           asioex::compose_tag<void(std::error_code)> = {})
    -> typename asio::async_result<std::decay_t<CompletionToken>,
                                    void(std::error_code)>::return_type
{
    const auto tk = asioex::compose_token(tk_);
    tim.expires_after(ms);
    auto [ec] = co_await tim.async_wait(tk);
    co_return asio::error_code{};
}

TEST_SUITE_BEGIN("async");

TEST_CASE("basics")
{
    int res;
    std::error_code ec, ec2;

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

    async_wait_none(tim,
               std::chrono::milliseconds(10),
               asio::bind_allocator(
                   alloc,
                   [&](std::error_code ec_)
                   {
                       ec2 = ec_;
                   }));

    auto ff = async_wait(tim,
                        std::chrono::milliseconds(10),
                        asio::use_future);

    auto f2 = async_wait_none(tim,
                         std::chrono::milliseconds(10),
                         asio::use_future);

    CHECK_NOTHROW(ctx.run());

    CHECK_THROWS(ff.get());
    CHECK_NOTHROW(f2.get());
    CHECK(res == 42);
    CHECK(ec == asio::error::host_not_found_try_again);
    CHECK(!ec2);

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
            self.complete({}, idx);
    }
};

template<typename CompletionToken>
void run_composed_op(asio::io_context & ctx, CompletionToken && token)
{
    asio::async_compose<CompletionToken, void(asio::error_code, std::size_t)>(
        test_composed_op{
        }, token, ctx);
}

template<typename CompletionToken>
auto async_benchmark(asio::io_context &ctx,
                     CompletionToken && tk_,
                     asioex::compose_tag<void(std::error_code, std::size_t)> = {})
    -> typename asio::async_result<std::decay_t<CompletionToken>,
                                         void(std::error_code, std::size_t)>::return_type
{


    std::size_t idx = 0u;
    for (; idx < 1000000u; idx++)
    {
        if (idx > 0u)
            assert(ctx.get_executor().running_in_this_thread());

        co_await asio::post(ctx.get_executor(), asioex::compose_token(tk_));
    }
    auto exec = co_await asio::this_coro::executor;
    co_return {{}, idx};
}



TEST_CASE("single op benchmark")
{
    using clock = std::chrono::steady_clock;
    using asio::detached;
    SUBCASE("naked composed op")
    {
        asio::io_context ctx;
        run_composed_op(ctx, detached);
        auto start = clock::now();
        ctx.run();
        auto end = clock::now();

        std::printf("Naked     composed op took %lldns\n", std::chrono::nanoseconds(end - start).count());
    }

    SUBCASE("recycling composed op")
    {
        asio::recycling_allocator<void> alloc;
        asio::io_context ctx;
        run_composed_op(ctx, asio::bind_allocator(alloc, detached));
        auto start = clock::now();
        ctx.run();
        auto end = clock::now();

        std::printf("Recycling composed op took %lldns\n", std::chrono::nanoseconds(end - start).count());
    }

    SUBCASE("naked async op")
    {
        asio::io_context ctx;
        async_benchmark(ctx, detached);
        auto start = clock::now();
        ctx.run();
        auto end = clock::now();

        std::printf("Naked         coro op took %lldns\n", std::chrono::nanoseconds(end - start).count());
    }

    SUBCASE("recycling async op")
    {
        asio::recycling_allocator<void> alloc;
        asio::io_context ctx;
        async_benchmark(ctx, asio::bind_allocator(alloc, detached));
        auto start = clock::now();
        ctx.run();
        auto end = clock::now();

        std::printf("Recycling     coro op took %lldns\n", std::chrono::nanoseconds(end - start).count());
    }
}

asio::awaitable<void> awaitable_impl()
try
{
    asio::steady_timer tim{co_await asio::this_coro::executor};
    try
    {
        co_await async_wait(tim,
                 std::chrono::milliseconds(10),
                 asio::use_awaitable);
        CHECK(false);
    }
    catch(asio::system_error  & se)
    {
        CHECK(se.code() == asio::error::host_not_found_try_again);
    }
    auto ec = std::get<0>(
                co_await async_wait_none(tim,
                    std::chrono::milliseconds(10),
                    asio::experimental::as_tuple(asio::use_awaitable)));
    CHECK(!ec);
    co_return ;
}
catch (...)
{
    REQUIRE(false);
}


TEST_CASE("awaitable")
{
    asio::io_context ctx;
    asio::co_spawn(ctx, awaitable_impl,
                   [](std::exception_ptr e)
                   {
                       CHECK(!e);
                   });
    ctx.run();
}

template<typename Exception, typename CompletionToken>
auto async_throw(asio::io_context &ctx,
                 Exception ex,
                 CompletionToken && tk_,
                 asioex::compose_tag<void(std::error_code)> = {})
    -> typename asio::async_result<std::decay_t<CompletionToken>,
                                    void(std::error_code)>::return_type
{
    co_await asio::post(ctx.get_executor(), asioex::compose_token(tk_));
    throw ex;
}

TEST_CASE("exception")
{
    asio::io_context ctx;
    async_throw(ctx, std::runtime_error("RT"), asio::detached);
    CHECK_THROWS_AS(ctx.run(), std::runtime_error);


    auto ex = [](std::exception_ptr e){CHECK(!e);};

    ctx.restart();
    CHECK_NOTHROW(ctx.run());

    ctx.restart();
    asio::co_spawn(ctx, async_throw(ctx, std::runtime_error("RT"), asio::use_awaitable), ex);
    CHECK_THROWS_AS(ctx.run(), std::runtime_error);
}

template<typename CompletionToken>
auto async_stop(asio::io_context &ctx,
                int & pos,
                bool & done,
                CompletionToken && tk_,
                asioex::compose_tag<void(std::error_code)> = {})
    -> typename asio::async_result<std::decay_t<CompletionToken>,
                                    void(std::error_code)>::return_type
{
    struct foobar
    {
        bool & done ;
        ~foobar()
        {
            done = true;
        }
    };

    foobar fb{done};

    pos = 1;
    co_await asio::post(ctx.get_executor(), asioex::compose_token(tk_));
    pos = 2;
    co_await asio::post(ctx.get_executor(), asioex::compose_token(tk_));
    pos = 3;

    co_return {};
}


TEST_CASE("run_one")
{
    int pos = 0;
    bool done = false;

    {
        asio::io_context ctx;
        async_stop(ctx, pos, done, asio::detached);
        CHECK(ctx.run_one() == 1u);
    }

    CHECK(pos == 2);
    CHECK(done);
}




TEST_SUITE_END();