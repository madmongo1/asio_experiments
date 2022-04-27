// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_MUTEX_HPP
#define ASIO_EXPERIMENTS_MUTEX_HPP

#include <asioex/async_semaphore.hpp>
#include <asio/experimental/append.hpp>
#include <asio/compose.hpp>


namespace asioex
{

template<typename Executor = asio::any_io_executor>
struct basic_mutex
{
    using executor_type = Executor;

    explicit basic_mutex(executor_type exec)
    : semaphore_(std::move(exec))
    {
    }

    template < ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken
                   ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type) >
    ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code))
    async_lock(CompletionToken &&token ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return asio::async_compose<CompletionToken, void(error_code)>(
            [&, did_suspend = false](auto& self, error_code ec = {}) mutable
            {
                if (ec)
                    std::move(self).complete(ec);
                else if (!locked_)
                {
                    locked_ = true;
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

    void
    unlock()
    {
        locked_ = false;
        semaphore_.release();
    }

    bool
    try_lock()
    {
        return std::exchange(locked_, true);
    }

    /// Rebinds the mutex type to another executor.
    template <typename Executor1>
    struct rebind_executor
    {
        /// The mutex type when rebound to the specified executor.
        typedef basic_mutex<Executor1> other;
    };

    template<typename Executor_>
    friend class basic_mutex;

    /// @brief return the default executor.
    executor_type const &
    get_executor() const {return semaphore_.get_executor();}

  private:
    bool              locked_ = false;
    basic_async_semaphore<Executor> semaphore_;
};

using mutex = basic_mutex<>;

template<typename Executor = asio::any_io_executor>
struct basic_lock_guard
{
    basic_lock_guard(const basic_lock_guard &) = delete;
    basic_lock_guard(basic_lock_guard &&lhs)
    : mtx_(std::exchange(lhs.mtx_, nullptr))
    {
    }

    basic_lock_guard &
    operator=(const basic_lock_guard &) = delete;
    basic_lock_guard &
    operator=(basic_lock_guard &&lhs)
    {
        std::swap(lhs.mtx_, mtx_);
        return *this;
    }

    ~basic_lock_guard()
    {
        if (mtx_)
            mtx_->unlock();
    }

    template<typename Executor_,  ASIO_COMPLETION_TOKEN_FOR(void(error_code, basic_lock_guard<Executor_>)) CompletionHandler>
    friend ASIO_INITFN_RESULT_TYPE(CompletionHandler, void(error_code, basic_lock_guard<Executor_>))
    async_guard(basic_mutex<Executor_> &mtx, CompletionHandler &&token);

  private:
    basic_lock_guard(basic_mutex<Executor> *mtx)
    : mtx_(mtx)
    {
    }
    basic_mutex<Executor> *mtx_ = nullptr;
};

using lock_guard = basic_lock_guard<>;

template<typename Executor,  ASIO_COMPLETION_TOKEN_FOR(void(error_code, basic_lock_guard<Executor>)) CompletionToken
               ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor) >
inline ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code, basic_lock_guard<Executor>))
    async_guard(basic_mutex<Executor> &mtx,
                CompletionToken &&token ASIO_DEFAULT_COMPLETION_TOKEN(Executor))
{
    return asio::async_compose<CompletionToken, void(error_code, basic_lock_guard<Executor>)>(
            [&](auto& self)
            {
                mtx.async_lock(
                    [&, s = std::move(self)](error_code ec) mutable
                    {
                        std::move(s).complete(ec, basic_lock_guard<Executor>(&mtx));
                    });
            }, token, mtx);
//    return mtx.async_lock(asio::experimental::append(basic_lock_guard<Executor>(&mtx)))(std::forward<CompletionHandler>(token));
}
}

#endif   // ASIO_EXPERIMENTS_MUTEX_HPP
