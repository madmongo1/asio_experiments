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

namespace asio::experimental
{
namespace st
{
namespace detail
{
template < class T >
struct expanding_circular_buffer
{
    static constexpr std::size_t initial_capacity = 16;

    void
    push(T p)
    {
        if (size_ == capacity_)
        {
            if (!storage_)
                init();
            else
                grow();
        }
        size_ += 1;
        storage_[back_pos_] = p;
        if (++back_pos_ >= capacity_)
            back_pos_ -= capacity_;
    }

    T
    pop()
    {
        assert(size_);
        auto result = storage_[front_pos_];
        if (++front_pos_ >= capacity_)
            front_pos_ -= capacity_;
        return result;
    }

    std::size_t
    size() const
    {
        return size_;
    }

  private:
    void
    init()
    {
        storage_   = std::make_unique< T[] >(initial_capacity);
        capacity_  = initial_capacity;
        front_pos_ = 0;
        back_pos_  = 0;
    }

    void
    grow()
    {
        if (capacity_ > std::numeric_limits< std::size_t >::max() / 2)
            throw std::bad_alloc();
        auto new_cap     = capacity_ * 2;
        auto new_storage = std::make_unique< T[] >(new_cap);

        std::size_t size = 0;

        // the front is ahead of the back (split buffer)
        if (back_pos_ <= front_pos_)
        {
            auto first = &new_storage[0];
            auto last =
                std::copy(&storage_[front_pos_], &storage_[capacity_], first);
            last = std::copy(&storage_[0], &storage_[back_pos_], last);
            size = std::distance(first, last);
            assert(size == size_);
        }
        else
        {
            auto first = &new_storage[0];
            auto last =
                std::copy(&storage_[front_pos_], &storage_[back_pos_], first);
            size = std::distance(first, last);
            assert(size == size_);
        }
        storage_.reset(std::move(new_storage));
        capacity_  = new_cap;
        front_pos_ = 0;
        back_pos_  = size;
    }

    std::size_t            capacity_  = 0;
    std::size_t            size_      = 0;
    std::size_t            front_pos_ = 0;
    std::size_t            back_pos_  = 0;
    std::unique_ptr< T[] > storage_;
};

struct semaphore_wait_op_concept
{
    virtual void
    invoke()                             = 0;
    virtual ~semaphore_wait_op_concept() = 0;
};

}   // namespace detail
template < class Executor = any_io_executor >
struct basic_async_semaphore
{
    using executor_type = Executor;

    /// Rebinds the socket type to another executor.
    template < typename Executor1 >
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef basic_async_semaphore< Executor1 > other;
    };

    basic_async_semaphore(executor_type exec, int initial_count = 1)
    : exec_(std::move(exec))
    , waiters_()
    , count_(initial_count)
    {
        assert(count_ >= 0);
    }

    executor_type const &
    get_executor() const
    {
        return exec_;
    }

    bool
    try_acquire()
    {
        bool acquired = false;
        if (count_ > 0)
        {
            --count_;
            acquired = true;
        }
        return acquired;
    }

    template < ASIO_COMPLETION_TOKEN_FOR(void()) CompletionHandler
                   ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type) >
    ASIO_INITFN_RESULT_TYPE(CompletionHandler, void())
    async_acquire(
        CompletionHandler &&token ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_initiate< CompletionHandler, void() >(
            [this]< class Handler >(Handler &&handler)
            {
                if (count_ > 0)
                {
                    --count_;
                    auto e = get_associated_executor(handler, get_executor());
                    post(std::move(e), std::forward< Handler >(handler));
                    return;
                }
                using handler_type = std::decay_t< Handler >;
                using model_type   = wait_op_model< handler_type >;
                auto halloc        = get_associated_allocator(handler);
                auto alloc =
                    typename std::allocator_traits< decltype(halloc) >::
                        template rebind_alloc< model_type >(halloc);
                auto traits = std::allocator_traits< decltype(alloc) >();
                auto pmem   = traits.allocate(alloc, sizeof(model_type));
                try
                {
                    auto pop = traits.construct(
                        alloc, pmem, std::forward< Handler >(handler));
                    try
                    {
                        waiters_.push(pop);
                    }
                    catch (...)
                    {
                        traits.destroy(alloc, pop);
                        throw;
                    }
                }
                catch (...)
                {
                    traits.deallocate(pmem);
                    throw;
                }
            },
            token);
    }

    void
    release()
    {
        count_ += 1;
        // release pending operations
    }

    template < class Handler >
    struct wait_op_model : detail::semaphore_wait_op_concept
    {
        using executor_type = associated_executor_t< Handler >;
        using cancellation_slot_type =
            associated_cancellation_slot_t< Handler >;
        using allocator_type = associated_allocator_t< Handler >;

        allocator_type
        get_allocator()
        {
            return get_associated_allocator(handler_);
        }

        cancellation_slot_type
        get_cancellation_slot()
        {
            return get_associated_cancellation_slot(handler_);
        }

        executor_type
        get_executor()
        {
            get_associated_cancellation_slot(handler_);
            return associated_executor(handler_);
        }

        wait_op_model(basic_async_semaphore *host, Handler handler)
        : host_(host)
        , handler_(std::move(handler))
        {
        }

        basic_async_semaphore *host_;
        Handler                handler_;
    };

    executor_type exec_;
    detail::expanding_circular_buffer< detail::semaphore_wait_op_concept * >
        waiters_;
    int count_;
};

using async_semaphore = basic_async_semaphore<>;

}   // namespace st
}   // namespace asio::experimental

using namespace asio;
using namespace asio::experimental;
using namespace std::literals;

awaitable< void >
co_sleep(std::chrono::milliseconds ms)
{
    auto t = use_awaitable.as_default_on(
        steady_timer(co_await this_coro::executor, ms));
    co_await t.async_wait();
}

awaitable< void >
bot(int n, st::async_semaphore &sem)
{
    auto ident = "bot " + std::to_string(n) + " : ";
    std::cout << ident << "approaching semaphore\n";
    if (!sem.try_acquire())
    {
        std::cout << ident << "waiting\n";
        co_await sem.async_acquire(use_awaitable);
    }

    std::cout << ident << "semaphore acquired\n";
    co_await co_sleep(1s);
    std::cout << ident << "work done\n";

    sem.release();
    std::cout << ident << "passed semaphore\n";
}

int
main()
{
    auto ioc = asio::io_context(ASIO_CONCURRENCY_HINT_UNSAFE);
    auto sem = st::async_semaphore(ioc.get_executor(), 10);
    for (int i = 0; i < 100; ++i)
        co_spawn(ioc, bot(i, sem), detached);
    ioc.run();
}
