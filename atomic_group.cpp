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
#include <asioex/concepts/transfer_latch.hpp>
#include <asioex/error_code.hpp>
#include <asioex/st/transfer_latch.hpp>

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

namespace asioex
{
template < concepts::transfer_latch Latch, class CompletionToken >
struct with_latch_t
{
    Latch          *latch_;
    CompletionToken token_;
};

template < concepts::transfer_latch Latch, class CompletionToken >
with_latch_t< Latch, std::decay_t< CompletionToken > >
latched(Latch &latch, CompletionToken &&token)
{
    return { .latch_ = &latch,
             .token_ = std::forward< CompletionToken >(token) };
}

template < class... Signatures >
struct latched_initiation
{
    template < class CompletionToken,
               asioex::concepts::transfer_latch Latch,
               class Initiation,
               class... InitArgs >
    void
    operator()(CompletionToken&& token,
               Latch          *latch,
               Initiation    &&initiation,
               InitArgs &&...init_args)
    {
        return asio::async_initiate< CompletionToken, Signatures... >(
            std::forward< Initiation >(initiation),
            token,
// uncomment to cause compile error            latch,
            std::forward< InitArgs >(init_args)...);
    }
};
}   // namespace asioex

namespace asio
{
template < class InnerToken, class... Signatures >
struct async_result<
    asioex::with_latch_t< asioex::st::transfer_latch, InnerToken >,
    Signatures... >
{
    template < typename Initiation,
               asioex::concepts::transfer_latch Latch,
               typename... InitArgs >
    static auto
    initiate(Initiation                              &&init,
             asioex::with_latch_t< Latch, InnerToken > my_token,
             InitArgs &&...init_args)
    {
        return asio::async_initiate< InnerToken, Signatures... >(
            asioex::latched_initiation< Signatures... > {},
            my_token.token_,
            my_token.latch_,
            std::forward< Initiation >(init),
            std::forward< InitArgs >(init_args)...);
    }
};

}   // namespace asio

template < class Executor, class Handler >
struct story_op : asio::coroutine
{
    asio::executor_work_guard< Executor > wg_;
    Handler                               handler_;

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

    story_op(Executor e, Handler h, std::span< std::string const > lines)
    : wg_(std::move(e))
    , handler_(std::move(h))
    , pstate()
    , lines(lines)
    {
    }

    using allocator_type = asio::associated_allocator_t< Handler >;

    using executor_type = Executor;

    allocator_type
    get_allocator() const
    {
        return asio::get_associated_allocator(handler_);
    }

    executor_type
    get_executor() const
    {
        return wg_.get_executor();
    }

#include <asio/yield.hpp>
    void operator()(asioex::error_code ec = {})
    {
        using asio::experimental::append;
        using namespace std::literals;

        reenter(this) for (;;)
        {
            if (!allocate_state(ec))
                return complete(ec);

            while (pstate->current != std::end(lines))
            {
                if (pstate->current != std::begin(lines))
                {
                    pstate->timer.expires_after(500ms);
                    ++yielded;
                    yield pstate->timer.async_wait(std::move(*this));
                    if (ec)
                        break;
                }
                println(*pstate->current++);
            }

            return complete(ec);
        }
    }
#include <asio/unyield.hpp>

    bool
    allocate_state(asioex::error_code &ec)
    {
        try
        {
            pstate = std::make_unique< state_t >(wg_.get_executor(), lines);
            ec.clear();
        }
        catch (std::bad_alloc &)
        {
            ec = asio::error::no_memory;
        }
        catch (std::exception &e)
        {
            ec = asio::error::invalid_argument;
        }
        return !ec;
    }

    void
    complete(asioex::error_code ec)
    {
        using asio::experimental::append;

        pstate.reset();

        if (yielded)
            return handler_(ec);
        else
        {
            auto e = get_executor();
            asio::post(e, append(std::move(handler_), ec));
        }
    }
};

struct initiate_story
{
    template < class Handler >
    void
    operator()(Handler &&handler, std::span< std::string const > lines) const
    {
        using exec_type =
            asio::associated_executor_t< Handler, asio::any_io_executor >;
        auto exec =
            asio::get_associated_executor(handler, asio::any_io_executor());
        auto op = story_op< exec_type, Handler >(
            std::move(exec), std::move(handler), lines);
        op();
    }

    template < class Handler, asioex::concepts::transfer_latch Latch >
    void
    operator()(Handler                      &&handler,
               Latch                         *latch,
               std::span< std::string const > lines) const
    {

        // for now ignore the latch

        using exec_type =
            asio::associated_executor_t< Handler, asio::any_io_executor >;
        auto exec =
            asio::get_associated_executor(handler, asio::any_io_executor());
        auto op = story_op< exec_type, Handler >(
            std::move(exec), std::move(handler), lines);
        op();
    }
};

template < ASIO_COMPLETION_TOKEN_FOR(void(asioex::error_code))
               CompletionHandler >
ASIO_INITFN_AUTO_RESULT_TYPE(CompletionHandler, void(asioex::error_code))
async_cat_story(CompletionHandler &&token)
{
    static const std::string lines[] = {
        "The", "cat", "sat", "on", "the", "mat"
    };

    return asio::async_initiate< CompletionHandler, void(asioex::error_code) >(
        initiate_story(), token, std::span(std::begin(lines), std::end(lines)));
}

template < ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) CompletionHandler >
ASIO_INITFN_AUTO_RESULT_TYPE(CompletionHandler, void(asio::error_code))
async_eggs_and_ham_story(CompletionHandler &&token)
{
    static const std::string lines[] = {
        "That Sam-I-am", "that Sam-I-am", "I do not like",
        "that Sam-I-am", "Do you like",   "green eggs and ham?"
    };

    return asio::async_initiate< CompletionHandler, void(asio::error_code) >(
        initiate_story(), token, std::span(std::begin(lines), std::end(lines)));
}

struct dummy_handler
{
    void operator()(asioex::error_code) {}
};

template < class... Jobs >
asio::awaitable< void >
sequential(Jobs &&...jobs)
{
    using asio::use_awaitable;

    asioex::st::transfer_latch latch;
    co_await async_eggs_and_ham_story(asioex::latched(latch, use_awaitable));

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

    //    auto tok = bind_executor(ioc, deferred);

    co_spawn(ioc,
             sequential(async_eggs_and_ham_story(deferred),
                        async_cat_story(deferred)),
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