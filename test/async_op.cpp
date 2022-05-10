// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <asioex/async_op.hpp>
#include "asio/post.hpp"
#include "asio/io_context.hpp"
#include "asio/experimental/append.hpp"
#include "asio/awaitable.hpp"
#include "asio/detached.hpp"
#include "asio/co_spawn.hpp"
#include <queue>

template<typename T>
asioex::async_op<void(std::exception_ptr, T)> make_dummy_op(asio::io_context & ctx, T value)
{
    return asio::co_spawn(ctx,
                          [=]() -> asio::awaitable<int>
                          {
                             printf("Producing %d\n", value);
                             co_return value;
                          }, asioex::as_async_op);
    //return asio::post(ctx, asio::experimental::append(asioex::as_async_op, value));
}

asio::awaitable<void> worker(std::queue<asioex::async_op<void(std::exception_ptr, int)>> ops)
{
    int i = 0;
    while (!ops.empty())
    {
        auto op = std::move(ops.front());
        ops.pop();
        bool was_posted = false;
        auto j = co_await op(asio::use_awaitable);
        printf("j == i [%d == %d]\n", j, i);
        assert(j == i);
        i++;
    }
}

int main(int argc, char * argv[])
{
    asio::io_context ctx;

    asioex::async_op<void()> tk = asio::post(ctx, asioex::as_async_op);
    bool called = false;
    tk([&]{called = true;});

    std::queue<asioex::async_op<void(std::exception_ptr, int)>> ops;

    for (int i = 0; i < 10; i ++)
        ops.push(make_dummy_op(ctx, i));

    asio::co_spawn(ctx, worker(std::move(ops)), asio::detached);

    ctx.run();
    assert(called);

    return 0;
}