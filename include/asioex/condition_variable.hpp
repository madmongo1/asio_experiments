// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_CONDITION_VARIABLE_HPP
#define ASIO_EXPERIMENTS_CONDITION_VARIABLE_HPP

#include <asioex/async_semaphore.hpp>
#include <asio/compose.hpp>

namespace asioex
{

template<typename Executor = asio::any_io_executor>
struct basic_condition_variable
{
    /// @brief The type of the default executor.
    using executor_type = Executor;

    /// @brief Construct a condition_variable
    /// @param exec is the default executor associated with the condition_variable
    explicit basic_condition_variable(executor_type exec)
    : semaphore_(std::move(exec), 0)
    {
    }

    /// @brief Initiate an asynchronous wait on the condition_variable
    template < ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken
                   ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type) >
    ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code))
    async_wait(CompletionToken &&token ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return semaphore_.async_acquire(std::forward< CompletionToken >(token));
    }

    /// @brief Initiate an asynchronous wait on the condition_variable with a predicate.
    template < typename Predicate,
                ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken
                   ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type) >
    ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code))
    async_wait(Predicate && predicate,
               CompletionToken &&token ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return asio::async_compose<CompletionToken, void(error_code)>(
            [&,
             predicate = std::move(predicate),
             did_suspend = false](auto& self, error_code ec = {}) mutable
            {
                if (ec)
                    std::move(self).complete(ec);
                else if (predicate())
                {
                    if (did_suspend)
                        std::move(self).complete(ec);
                    else
                        asio::post(
                            get_associated_executor(self, get_executor()),
                            [s = std::move(self)]() mutable
                            {
                                std::move(s).complete(error_code{});
                            });
                }
                else
                {
                    did_suspend = true;
                    semaphore_.async_acquire(std::move(self));
                }
            }, token, *this);
    }

    /// Notify one waitor.
    void
    notify_one()
    {
        semaphore_.release();
    }

    /// Notify everyone waiting for the condition_variable.
    void
    notify_all()
    {
        semaphore_.release_all();
    }

    /// Rebinds the condition_variable type to another executor.
    template <typename Executor1>
    struct rebind_executor
    {
        /// The condition_variable type when rebound to the specified executor.
        typedef basic_condition_variable<Executor1> other;
    };

    template<typename Executor_>
    friend class basic_condition_variable;

    /// @brief return the default executor.
    executor_type const &
    get_executor() const {return semaphore_.get_executor();}

  private:
    basic_async_semaphore<Executor> semaphore_;
};

using condition_variable = basic_condition_variable<>;

}

#endif   // ASIO_EXPERIMENTS_CONDITION_VARIABLE_HPP
