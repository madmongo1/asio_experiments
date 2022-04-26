// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <asioex/mutex.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/steady_timer.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include "cmake-build-debug/_deps/asio-src/asio/include/asio/detached.hpp"

asio::awaitable<void> main_impl()
{
    using executor = asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>;
    asioex::basic_mutex<executor> mtx { co_await asio::this_coro::executor };

    std::vector< int > seq;

    auto f = [](std::vector< int > &v, asioex::basic_mutex<executor> &mtx, int i) -> asio::awaitable< void >
    {
        auto l = co_await asioex::async_guard(mtx);
        v.push_back(i);
        asio::steady_timer tim { co_await asio::this_coro::executor, std::chrono::milliseconds(10) };
        co_await tim.async_wait(asio::use_awaitable);
        v.push_back(i + 1);
    };

    using asio::experimental::awaitable_operators::operator&&;

    co_await (f(seq, mtx, 0) && f(seq, mtx, 3) && f(seq, mtx, 6) && f(seq, mtx, 9));

    assert(seq.size() == 8);
    assert(seq[0] + 1 == seq[1]);
    assert(seq[2] + 1 == seq[3]);
    assert(seq[4] + 1 == seq[5]);
    assert(seq[6] + 1 == seq[7]);
}

int main(int argc, char * argv[])
{
    asio::io_context ctx;
    asio::co_spawn(ctx, main_impl(), asio::detached);
    ctx.run();
    return 0;
}