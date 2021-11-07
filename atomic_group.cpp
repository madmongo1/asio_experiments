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
#include <asio/experimental/deferred.hpp>
#include <asioex/error_code.hpp>

#include <iostream>
#include <memory>
#include <memory_resource>
#include <span>

std::mutex iom;
template < class... Ts >
void
print(Ts &&...ts)
{
    auto lg = std::lock_guard(iom);
    ((std::cout << ts), ...);
}

template < class... Ts >
void
println(Ts &&...ts)
{
    print(std::forward< Ts >(ts)..., '\n');
}

struct story_op : asio::coroutine
{
    struct state_t
    {
        state_t(asio::any_io_executor                 exec,
                std::span< std::string const > const &lines)
        : timer(std::move(exec))
        , current(std::begin(lines))
        {
        }
        asio::steady_timer                       timer;
        std::span< std::string const >::iterator current;
    };

    std::unique_ptr< state_t >     pstate;
    std::span< std::string const > lines;
    int                            yielded = 0;

    story_op(std::span< std::string const > lines)
    : pstate()
    , lines(lines)
    {
    }

#include <asio/yield.hpp>
    template < class Self >
    void operator()(Self &&self, asioex::error_code ec = {})
    {
        using asio::experimental::append;
        using namespace std::literals;

        reenter(this) for (;;)
        {
            pstate = std::make_unique< state_t >(
                asio::get_associated_executor(self), lines);
            if (!pstate)
            {
                yield asio::post(append(std::move(self), ec));
                return self.complete(ec);
            }

            while (pstate->current != std::end(lines))
            {
                if (pstate->current != std::begin(lines))
                {
                    pstate->timer.expires_after(500ms);
                    ++yielded;
                    yield pstate->timer.async_wait(std::move(self));
                    if (ec)
                        break;
                }
                println(*pstate->current++);
            }

            pstate.reset();
            if (!yielded)
                yield asio::post(append(std::move(self), ec));
            return self.complete(ec);
        }
    }
#include <asio/unyield.hpp>
};

template < ASIO_COMPLETION_TOKEN_FOR(void(asioex::error_code))
               CompletionHandler >
ASIO_INITFN_RESULT_TYPE(CompletionHandler, void(asioex::error_code))
async_cat_story(CompletionHandler &&token)
{
    static const std::string lines[] = {
        "The", "cat", "sat", "on", "the", "mat"
    };

    return asio::async_compose< CompletionHandler, void(asioex::error_code) >(
        story_op(std::span(std::begin(lines), std::end(lines))), token);
}

template < ASIO_COMPLETION_TOKEN_FOR(void(asioex::error_code))
               CompletionHandler >
ASIO_INITFN_RESULT_TYPE(CompletionHandler, void(asioex::error_code))
async_eggs_and_ham_story(CompletionHandler &&token)
{
    static const std::string lines[] = {
        "That Sam-I-am", "that Sam-I-am", "I do not like",
        "that Sam-I-am", "Do you like",   "green eggs and ham?"
    };

    return asio::async_compose< CompletionHandler, void(asioex::error_code) >(
        story_op(std::span(std::begin(lines), std::end(lines))), token);
}

template < class... Jobs >
asio::awaitable< void >
sequential(Jobs &&...jobs)
{
    using asio::use_awaitable;

    (co_await jobs(use_awaitable), ...);
}

int
main()
{
    using asio::bind_executor;
    using asio::co_spawn;
    using asio::detached;
    using asio::experimental::deferred;

    auto ioc = asio::io_context();
    auto tp  = asio::thread_pool();

    auto tok = bind_executor(ioc, deferred);

    co_spawn(ioc,
             sequential(async_eggs_and_ham_story(tok), async_cat_story(tok)),
             detached);
    /*
    async_cat_story(asio::bind_executor(
        ioc, [](asioex::error_code) { println("End of cat story\n"); }));
    async_eggs_and_ham_story(asio::bind_executor(
        ioc,
        [](asioex::error_code) { println("Dr Seuss is not cancelled\n"); }));
*/
    ioc.run();
    tp.wait();
}