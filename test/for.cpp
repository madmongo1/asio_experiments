// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <asioex/for.hpp>
#include <asio.hpp>

#include <asio/experimental/channel.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

asio::awaitable<std::size_t> for_chan_test(asio::experimental::channel<void(asio::error_code, int)> & chan)
{
    std::size_t sz = 0u;
    try
    {
        co_for (value, chan, asio::use_awaitable)
        {
            printf("for_chan_test: %d %d\n", (int)value,  (int)sz);
            CHECK(value == sz++);
        }
    }
    catch(asio::system_error & se)
    {
        CHECK(se.code() == asio::error::fault);
    }
    co_return sz;
}

asio::experimental::coro<int> cr(asio::any_io_executor exec)
{
    co_yield 10;
    co_yield 11;
    co_yield 12;
    co_yield 13;
    co_yield 14;
}

asio::awaitable<std::size_t> for_coro_test(asio::experimental::coro<int> cr)
{
    std::size_t sz = 10u;

    co_for (value, cr, asio::use_awaitable)
    {
        printf("for_coro_test: %d %d\n", (int)value,  (int)sz);
        CHECK(value == sz++);
    }
    co_return sz;
}



TEST_CASE("for")
{
    asio::io_context ctx;
    asio::experimental::channel<void(asio::error_code, int)> chan{ctx};
    asio::co_spawn(ctx, for_chan_test(chan),
                   [](std::exception_ptr e, std::size_t sz)
                   {
                       CHECK(!e);
                       CHECK(sz == 5);
                   });

    asio::co_spawn(ctx, for_coro_test(cr(ctx.get_executor())),
                   [](std::exception_ptr e, std::size_t sz)
                   {
                       CHECK(!e);
                       CHECK(sz == 15);
                   });



    chan.async_send(asio::error_code{}, 0, asio::detached);
    chan.async_send(asio::error_code{}, 1, asio::detached);
    chan.async_send(asio::error_code{}, 2, asio::detached);
    chan.async_send(asio::error_code{}, 3, asio::detached);
    chan.async_send(asio::error_code{}, 4, asio::detached);
    chan.async_send(asio::error::fault, 5, asio::detached);

    ctx.run();
}