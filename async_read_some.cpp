//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <asioex/async_read_some.hpp>
#include <asioex/latched_completion.hpp>
#include <asioex/st/transfer_latch.hpp>
#include <asioex/transaction.hpp>

#include <iostream>

asio::awaitable< void >
connect_pair(asio::ip::tcp::socket &client, asio::ip::tcp::socket &server);

template < asioex::concepts::transfer_latch Latch >
asio::awaitable< void >
trip_latch_after(Latch &latch, std::chrono::nanoseconds delay);

asio::awaitable< void >
test_atomic_op()
{
    using namespace std::literals;
    using namespace asio::experimental::awaitable_operators;
    using tcp = asio::ip::tcp;
    using asio::use_awaitable;
    using asio::experimental::as_tuple;

    auto e = co_await asio::this_coro::executor;

    auto client = tcp::socket(e);
    auto server = tcp::socket(e);
    co_await connect_pair(client, server);

    char buffer[1024];
    auto latch = asioex::st::transfer_latch();

#if 1
    std::string tx = "Hello";
    client.write_some(asio::buffer(tx));

#endif
    try
    {
        auto which =
            co_await(asioex::async_read_some(
                         server,
                         asio::buffer(buffer),
                         asioex::latched_completion(latch, use_awaitable)) ||
                     trip_latch_after(latch, 1ms));

        switch (which.index())
        {
        case 0:
            std::cout << __func__ << ": read completed\n";
            break;
        case 1:
            std::cout << __func__ << ": timeout\n";
            break;
        }
    }
    catch (std::exception &e)
    {
        std::cout << __func__ << ": exception: " << e.what() << std::endl;
    }
}

int
main()
{
    auto ioc = asio::io_context();
    asio::co_spawn(ioc, test_atomic_op(), asio::detached);
    ioc.run();
}

// support functions

asio::awaitable< void >
connect_pair(asio::ip::tcp::socket &client, asio::ip::tcp::socket &server)
{
    using asio::ip::address_v4;
    using tcp = asio::ip::tcp;
    using asio::use_awaitable;
    using namespace asio::experimental::awaitable_operators;

    auto e        = client.get_executor();
    auto acceptor = tcp::acceptor(e, tcp::endpoint(address_v4::loopback(), 0));
    co_await(acceptor.async_accept(server, use_awaitable) &&
             client.async_connect(acceptor.local_endpoint(), use_awaitable));

    co_return;
}

template < asioex::concepts::transfer_latch Latch >
asio::awaitable< void >
trip_latch_after(Latch &latch, std::chrono::nanoseconds delay)
{
    auto timer = asio::steady_timer(co_await asio::this_coro::executor, delay);
    co_await timer.template async_wait(
        asio::experimental::as_tuple(asio::use_awaitable));
    auto trans = asioex::begin_transaction(latch);
    if (trans.may_commit())
        trans.commit();
}
