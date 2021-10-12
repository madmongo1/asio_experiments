//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/blog-2021-10
//

#include <asio.hpp>
#include <asio/experimental/deferred.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/parallel_group.hpp>
#ifdef WIN32
#include <asio/experimental/awaitable_operators.hpp>
#endif
#include <iostream>
#include <iomanip>

template <typename CompletionToken>
struct timed_token
{
    std::chrono::milliseconds timeout;
    CompletionToken& token;
};

// Note: this is merely a function object - a lambda.
template <typename... Signatures>
struct timed_initiation
{
    template <
        typename CompletionHandler,
        typename Initiation,
        typename... InitArgs>
    void operator()(
      CompletionHandler handler,         // the generated completion handler
      std::chrono::milliseconds timeout, // the timeout specified in our completion token
      Initiation&& initiation,           // the embedded operation's initiation (e.g. async_read)
      InitArgs&&... init_args)           // the arguments passed to the embedded initiation (e.g. the async_read's buffer argument etc)
    {
        using asio::experimental::make_parallel_group;

        // locate the correct executor associated with the underling operation
        // first try the associated executor of the handler. If that doesn't
        // exist, take the associated executor of the underlying async operation's handler
        // If that doesn't exist, use the default executor (system executor currently)
        auto ex =
            asio::get_associated_executor(
                handler,
                asio::get_associated_executor(initiation));

        // build a timer object and own it via a shared_ptr. This is because its
        // lifetime is shared between two asynchronous chains. Use the handler's
        // allocator in order to take advantage of the Asio recycling allocator.
        auto alloc =
            asio::get_associated_allocator(handler);

        std::shared_ptr<asio::steady_timer> timer =
            std::allocate_shared<asio::steady_timer>(alloc, ex, timeout);

        // launch a parallel group of asynchronous operations - one for the timer
        // wait and one for the underlying asynchronous operation (i.e. async_read)
        make_parallel_group(
                // item 0 in the group is the timer wait
                asio::bind_executor(ex,
                                    [&](auto&& token)
                                    {
                                        return timer->async_wait(std::forward<decltype(token)>(token));
                                    }),
                // item 1 in the group is the underlying async operation
                asio::bind_executor(ex,
                                    [&](auto&& token)
                                    {
                                        // Finally, initiate the underlying operation
                                        // passing its original arguments
                                        return asio::async_initiate<decltype(token), Signatures...>(
                                                std::forward<Initiation>(initiation), token,
                                                std::forward<InitArgs>(init_args)...);
                                    })
        ).async_wait(
                // Wait for the first item in the group to complete. Upon completion
                // of the first, cancel the others.
                asio::experimental::wait_for_one(),

                // The completion handler for the group
                [handler = std::move(handler), timer](
                    // an array of indexes indicating in which order the group's
                    // operations completed, whether successfully or not
                    std::array<std::size_t, 2>,

                    // The arguments are the result of concatenating
                    // the completion handler arguments of all operations in the
                    // group, in retained order:
                    // first the steady_timer::async_wait
                    std::error_code,

                    // then the underlying operation e.g. async_read(...)
                    auto... underlying_op_results // e.g. error_code, size_t
                    ) mutable
                {
                    // release all memory prior to invoking the final handler
                    timer.reset();
                    // finally, invoke the handler with the results of the
                    // underlying operation
                    std::move(handler)(std::move(underlying_op_results)...);
                });
    }
};

// Specialise the async_result primary template for our timed_token
template <typename InnerCompletionToken, typename... Signatures>
struct asio::async_result<
      timed_token<InnerCompletionToken>,  // specialised on our token type
      Signatures...>
{
    // The implementation will call initiate on our template class with the
    // following arguments:
    template <typename Initiation, typename... InitArgs>
    static auto initiate(
        Initiation&& init, // This is the object that we invoke in order to
                           // actually start the packaged async operation
        timed_token<InnerCompletionToken> t, // This is the completion token that
                                             // was passed as the last argument to the
                                             // initiating function
        InitArgs&&... init_args)      // any more arguments that were passed to
                                      // the initiating function
    {
        // we will customise the initiation through our token by invoking
        // async_initiate with our own custom function object called
        // timed_initiation. We will pass it the arguments that were passed to
        // timed(). We will also forward the initiation created by the underlying
        // completion token plus all arguments destined for the underlying
        // initiation.
        return asio::async_initiate<InnerCompletionToken, Signatures...>(
                timed_initiation<Signatures...>{},
                  t.token,   // the underlying token
                  t.timeout, // our timeout argument
                std::forward<Initiation>(init),  // the underlying operation's initiation
                std::forward<InitArgs>(init_args)... // that initiation's arguments
        );
    }
};

template <typename CompletionToken>
timed_token<CompletionToken>
timed(std::chrono::milliseconds timeout, CompletionToken&& token)
{
    return timed_token<CompletionToken>{ timeout, token };
}

template <typename Op, typename CompletionToken>
auto with_timeout(Op op, std::chrono::milliseconds timeout, CompletionToken&& token)
{
    return std::move(op)(timed(timeout, std::forward<CompletionToken>(token)));
}

template <typename Op>
asio::awaitable<void> test(Op op)
{
    co_await op(asio::use_awaitable);
}

std::string_view trim_crlf(std::string_view sv)
{
  while(sv.size() && (sv.back() == '\r' || sv.back() == '\n'))
    sv = sv.substr(0, sv.size() - 1);
  return sv;
}

std::string_view
left_view(std::string const& s, std::size_t n)
{
  return { s.data(), n };
}

asio::awaitable<
        std::tuple<
                asio::ip::tcp::socket,
                asio::ip::tcp::socket>>
local_socket_pair()
{
    namespace this_coro = asio::this_coro;
    using namespace asio::experimental::awaitable_operators;
    using asio::use_awaitable;

    auto acceptor =
            asio::ip::tcp::acceptor (
                    co_await this_coro::executor,
                    asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));

    acceptor.listen();

    auto client = asio::ip::tcp::socket(co_await this_coro::executor);
    auto server = asio::ip::tcp::socket(co_await this_coro::executor);
    co_await (
            acceptor.async_accept(server, use_awaitable) &&
            client.async_connect(acceptor.local_endpoint(), use_awaitable)
    );

    co_return std::make_tuple(std::move(client), std::move(server));
}

asio::awaitable<void>
send_test_data(asio::ip::tcp::socket sock_)
{
    using namespace std::literals;
    auto sock = asio::use_awaitable.as_default_on(std::move(sock_));
    auto timer = asio::use_awaitable.as_default_on(asio::steady_timer { sock.get_executor(), 1s });

    // wait for 1 second before writing to the server
    co_await timer.async_wait();
    auto message = "Hello, World! - next message is in 6 seconds.\n"s;
    co_await asio::async_write(sock, asio::buffer(message));

    // now wait 6 seconds so the second read times out
    timer.expires_after(6s);
    co_await timer.async_wait();

    message = "You shouldn't see this.";
    co_await asio::async_write(sock, asio::buffer(message));
}

asio::awaitable<void> run()
{
    using namespace asio;
    using namespace std::literals;
    using experimental::deferred;
    using experimental::as_tuple;
#ifdef WIN32
    auto [client, in] = co_await local_socket_pair();
    asio::co_spawn(co_await this_coro::executor, send_test_data(std::move(client)), asio::detached);
#else
    posix::stream_descriptor in(co_await this_coro::executor, ::dup(STDIN_FILENO));
#endif
    std::string line;
    std::cout << "using the token: ";
    std::cout.flush();
    auto [ec1, n1] = co_await async_read_until(in, dynamic_buffer(line), '\n',
                                              as_tuple(timed(5s, use_awaitable)));
    std::cout << "error: " << std::quoted(ec1.message())
              << " message: " << std::quoted(trim_crlf(left_view(line, n1))) << std::endl;
    line.erase(0, n1);

    std::cout << "using with_timeout(): ";
    std::cout.flush();
    auto [ec2, n2] = co_await with_timeout(
            async_read_until(in, asio::dynamic_buffer(line), '\n', deferred),
            5s,
            as_tuple(use_awaitable));
    std::cout << "error: " << std::quoted(ec2.message())
              << " message: " << std::quoted(trim_crlf(left_view(line, n2))) << "\n";
    line.erase(0, n2);
}

int main()
{
    asio::io_context ctx;
    co_spawn(ctx, run(), asio::detached);
    ctx.run();
}