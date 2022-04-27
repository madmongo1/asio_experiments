//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <asio.hpp>

#include <asio/experimental/append.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asioex/async_semaphore.hpp>

#include <iostream>
#include <random>

namespace asioex
{
namespace experimental
{
}   // namespace experimental
}   // namespace asioex

using namespace asio;
using namespace asio::experimental;
using namespace std::literals;
using namespace experimental::awaitable_operators;
using namespace asioex;

awaitable< void >
co_sleep(std::chrono::milliseconds ms)
{
    auto t = use_awaitable.as_default_on(
        steady_timer(co_await this_coro::executor, ms));
    co_await t.async_wait();
}

awaitable< void >
timeout(std::chrono::milliseconds ms)
{
    auto t = steady_timer(co_await this_coro::executor, ms);
    co_await t.async_wait(as_tuple(use_awaitable));
}

awaitable< void >
bot(int n, async_semaphore &sem, std::chrono::milliseconds deadline)
{
    auto say = [ident = "bot " + std::to_string(n) + " : "](auto &&...args)
    {
        std::cout << ident;
        ((std::cout << args), ...);
    };

    if (say("approaching semaphore\n"); !sem.try_acquire())
    {
        say("waiting up to ", deadline.count(), "ms\n");
        auto then = std::chrono::steady_clock::now();
        if ((co_await(sem.async_acquire(as_tuple(use_awaitable)) ||
                      timeout(deadline)))
                .index() == 1)
        {
            say("got bored waiting after ", deadline.count(), "ms\n");
            co_return;
        }
        else
        {
            say("semaphore acquired after ",
                std::chrono::duration_cast< std::chrono::milliseconds >(
                    std::chrono::steady_clock::now() - then)
                    .count(),
                "ms\n");
        }
    }
    else
    {
        say("semaphore acquired immediately\n");
    }

    co_await co_sleep(500ms);
    say("work done\n");

    sem.release();
    say("passed semaphore\n");
}

#define check_eq(X, Y) \
    if (X != Y)        \
    {                  \
        printf(#X " == " #Y " failed: %d != %d\n", X, Y); \
        errors++;               \
    }

int test_value()
{
    int errors = 0;

    asio::io_context ioc;
    async_semaphore sem{ioc.get_executor(), 0};


    check_eq(sem.value(), 0);

    sem.release();
    check_eq(sem.value(), 1);
    sem.release();
    check_eq(sem.value(), 2);

    sem.try_acquire();
    check_eq(sem.value(), 1);

    sem.try_acquire();
    check_eq(sem.value(), 0);

    sem.async_acquire(asio::detached);
    check_eq(sem.value(), -1);
    sem.async_acquire(asio::detached);
    check_eq(sem.value(), -2);

    return errors;
}

int
main()
{
    int res = 0;
    res += test_value();

    auto ioc  = asio::io_context(ASIO_CONCURRENCY_HINT_UNSAFE);
    auto sem  = async_semaphore(ioc.get_executor(), 10);
    auto rng  = std::random_device();
    auto ss   = std::seed_seq { rng(), rng(), rng(), rng(), rng() };
    auto eng  = std::default_random_engine(ss);
    auto dist = std::uniform_int_distribution< unsigned int >(1000, 10000);

    auto random_time = [&eng, &dist]
    { return std::chrono::milliseconds(dist(eng)); };
    for (int i = 0; i < 100; i += 2)
        co_spawn(ioc, bot(i, sem, random_time()), detached);
    ioc.run();
    return res;
}
