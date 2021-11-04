//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <asio.hpp>

#include <iostream>
#include <optional>
#include <thread>

std::mutex iom;
template < class... Args >
void
emit(Args const &...args)
{
    auto l = std::lock_guard(iom);
    ((std::cout << args), ...);
}

template<class Sig>
struct completion_handler_guard_state;

template<class...Args>
struct completion_handler_guard_state<void(Args...)>
{
    virtual void complete(Args...args) = 0;
    virtual  ~completion_handler_guard_state() = default;
};

template<class Handler, class Sig>
struct completion_handler_guard_state_impl;

template<class Handler, class...Args>
struct completion_handler_guard_state_impl<Handler, void(Args...)>
{
    void complete(Args...args) override;

    Handler handler_;
};

template < class Sig >
struct completion_handler_guard;

template < class... Args >
struct completion_handler_guard< void(Args...) >
{
    virtual void
    operator()(Args... args) = 0;
};

template < class... Args >
struct completion_handler_guard< void(asio::error_code, Args...) >
{
    virtual void
    operator()(asio::error_code, Args... args) = 0;
};

template < class... Args >
struct completion_handler_guard< void(std::exception_ptr, Args...) >
{
    virtual void
    operator()(std::exception_ptr, Args... args) = 0;
};

template < class Handler, class Sig >
struct completion_handler_guard_impl;

template < class Handler, class... Args >
struct completion_handler_guard_impl< Handler, void(asio::error_code, Args...) >
: completion_handler_guard< void(asio::error_code, Args...) >
{
    ~completion_handler_guard_impl()
    {
        if (handler_)
        {
            auto h = std::move(*handler_);
            handler_.reset();
            h(asio::error::broken_pipe, Args()...);
        }
    }

    virtual void
    operator()(asio::error_code ec, Args... args) override
    {
        assert(handler_);
        auto h = std::move(*handler_);
        handler_.reset();
        h(ec, std::move(args...));
    }

    std::optional< Handler > handler_;
};

void
succeed(completion_handler_guard<void(asio::error_code, std::string)> complete)
{
    complete(asio::error_code(), "Hello, World!");
}

void
fail(completion_handler_guard<void(asio::error_code, std::string)> complete)
{
    complete(asio::error::eof, std::string());
}

void
dont_complete(completion_handler_guard<void(asio::error_code, std::string)> complete)
{
}

int
main()
{
    auto workers = asio::thread_pool(1);
    auto wg      = asio::make_work_guard(workers.get_executor());

    auto ioc = asio::io_context();

    asio::post(workers,
               []
               { emit("worker thread: ", std::this_thread::get_id(), "\n"); });
    asio::post(ioc,
               [] { emit("io thread: ", std::this_thread::get_id(), "\n"); });

    ioc.run();
    wg.reset();
    workers.wait();
}