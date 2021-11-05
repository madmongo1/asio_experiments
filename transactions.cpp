//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <asio.hpp>

#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asioex/error.hpp>
#include <asioex/mt/transfer_latch.hpp>
#include <asioex/st/transfer_latch.hpp>
#include <asioex/transaction.hpp>

#include <iostream>
#include <variant>

namespace asioex
{
static_assert(concepts::basic_lockable< std::mutex >);
static_assert(concepts::basic_lockable< null_mutex >);
static_assert(concepts::transfer_latch< mt::transfer_latch >);
static_assert(concepts::transfer_latch< st::transfer_latch >);

}   // namespace asioex

void
test2()
{
    asioex::mt::transfer_latch l1, l2;

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        assert(l1.may_commit());
        assert(l2.may_commit());
        t1.rollback();
    }

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        assert(l1.may_commit());
        assert(l2.may_commit());
        t1.commit();
    }

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        assert(!l1.may_commit());
        assert(!l2.may_commit());
        assert(!t1.may_commit());
    }
}

void
test1()
{
    asioex::mt::transfer_latch l1;

    assert(l1.may_commit());

    {
        auto t1 = asioex::begin_transaction(l1);
        assert(l1.may_commit());
        t1.rollback();
    }

    {
        auto t1 = asioex::begin_transaction(l1);
        assert(l1.may_commit());
        t1.commit();
    }

    {
        auto t1 = asioex::begin_transaction(l1);
        assert(!l1.may_commit());
        assert(!t1.may_commit());
    }
}

namespace asioex
{
template < concepts::transfer_latch Latch, class CompletionToken >
struct latch_and_completion_token
{
    Latch          &latch;
    CompletionToken token;
};

template < asioex::concepts::transfer_latch Latch >
struct atomic_read_op : asio::coroutine
{
    asio::ip::tcp::socket  &sock;
    asio::mutable_buffers_1 buf;
    Latch                  &latch;

    Latch &
    get_transfer_latch() const
    {
        return latch;
    }

#include <asio/yield.hpp>
    template < class Self >
    void operator()(Self &&self, std::error_code ec = {}, std::size_t size = 0)
    {
        reenter(this) for (;;)
        {
            yield sock.async_wait(asio::socket_base::wait_type::wait_read,
                                  std::move(self));

            auto trans = begin_transaction(latch);
            if (!trans.may_commit())
            {
                trans.rollback();
                return self.complete(error::completion_denied, 0);
            }
            if (ec)
            {
                trans.commit();
                return self.complete(ec, size);
            }

            auto wasblocked = sock.non_blocking();
            sock.non_blocking(true);
            size = sock.read_some(buf, ec);
            sock.non_blocking(wasblocked);
            if (ec == asio::error::would_block)
            {
                ec.clear();
                trans.rollback();
                continue;
            }
            trans.commit();
            return self.complete(ec, size);
        }
    }
#include <asio/unyield.hpp>
};

template < ASIO_COMPLETION_TOKEN_FOR(void(std::error_code, std::size_t))
               ReadHandler,
           asioex::concepts::transfer_latch Latch >
ASIO_INITFN_RESULT_TYPE(ReadHandler, void(std::error_code, std::size_t))
async_atomic_read_some(asio::ip::tcp::socket  &sock,
                       asio::mutable_buffers_1 buf,
                       Latch                  &latch,
                       ReadHandler           &&token)
{
    return asio::async_compose< ReadHandler,
                                void(std::error_code, std::size_t) >(
        atomic_read_op< Latch > { .sock = sock, .buf = buf, .latch = latch },
        token,
        sock);
}

}   // namespace asioex

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
            co_await(asioex::async_atomic_read_some(
                         server, asio::buffer(buffer), latch, use_awaitable) ||
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
    test1();
    test2();

    auto ioc = asio::io_context();
    asio::co_spawn(ioc, test_atomic_op(), asio::detached);
    ioc.run();
}
