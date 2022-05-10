// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <asioex/condition_variable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/io_context.hpp>

#define check(Cond) if (!Cond) { printf(__FILE__ "(%d): " #Cond " failed\n", __LINE__); errors++; }


asio::awaitable<int> main_impl()
{
    int errors = 0;
    asioex::condition_variable cond(co_await asio::this_coro::executor);

    bool fired = false;

    cond.async_wait([&](asio::error_code ec){ assert(!ec); fired = true;});

    check(!fired);
    co_await asio::post(asio::use_awaitable);
    check(!fired);
    cond.notify_one();
    co_await asio::post(asio::use_awaitable);
    check(fired);

    bool pred = false;
    fired = false;
    cond.async_wait([&]{ return pred;},
                    [&](asio::error_code ec){ assert(!ec); fired = true;});

    check(!fired);

    cond.notify_all();
    co_await asio::post(asio::use_awaitable);
    check(!fired);

    pred = true;
    cond.notify_all();
    co_await asio::post(asio::use_awaitable);
    check(fired);

    co_return errors;
}

int main(int argc, char * argv[])
{
    int res = 0;
    asio::io_context ctx;

    asio::co_spawn(
        ctx, main_impl(),
        [&res](std::exception_ptr e, int res_)
        {
           if (e)
               std::rethrow_exception(e);
           res = res_;
        });

    ctx.run();
    return res;
}