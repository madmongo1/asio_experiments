//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <asio.hpp>

#include <asio/experimental/deferred.hpp>

#include <array>
#include <iostream>
#include <mutex>
#include <thread>
#include <tuple>

std::mutex iom;

template < class... Ts >
void
emit(Ts const &...xs)
{
    auto l = std::lock_guard(iom);
    ((std::cout << xs), ...);
    std::cout.flush();
}

template < class... Contexts >
struct parallel_contexts
{
    parallel_contexts(Contexts &...contexts)
    : contexts_(std::tie(contexts...))
    {
    }

    void
    run()
    {
        run_impl(std::make_index_sequence< sizeof...(Contexts) >());
    }

    void
    wait()
    {
        for (auto &thread : threads_)
            if (thread.joinable())
                thread.join();
    }

  private:
    template < std::size_t... Is >
    void run_impl(std::index_sequence< Is... >)
    {
        auto op =
            [this]< std::size_t I >(std::integral_constant< std::size_t, I >)
        {
            auto &context = get< I >(contexts_);
            auto &thread  = get< I >(threads_);

            thread = std::jthread(
                [&context, this]
                {
                    emit(std::this_thread::get_id(),
                         " : ",
                         I,
                         " : starting",
                         '\n');
                    context.run();
                    emit(std::this_thread::get_id(),
                         " : ",
                         I,
                         " : stopping",
                         '\n');
                });
        };
        (op(std::integral_constant< std::size_t, Is >()), ...);
    }

    std::tuple< Contexts &... >                     contexts_;
    std::array< std::jthread, sizeof...(Contexts) > threads_;
};

using asio::experimental::deferred;
using asio::use_awaitable;
using namespace std::literals;

int
main()
{
    auto ioc1 = asio::io_context();
    auto ioc2 = asio::io_context();

    auto iocs = parallel_contexts(ioc1, ioc2);

    auto sequence_on1 = asio::dispatch(ioc1, deferred);
    auto sequence_on2 = asio::post(ioc2, deferred);

    auto operation = []()
    { emit(std::this_thread::get_id(), " : Operation\n"); };
    auto completion = []()
    { emit(std::this_thread::get_id(), " : Completion\n"); };

    sequence_on1(
        [operation, completion, sequence_on2]()
        {
            operation();
            sequence_on2(completion);
        });

    auto g2 = asio::make_work_guard(ioc2);

    iocs.run();

    std::this_thread::sleep_for(1s);

    sequence_on2([] { emit(std::this_thread::get_id(), " : Done\n"); });
    g2.reset();

    iocs.wait();
}