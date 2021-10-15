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
        size_ -= 1;
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
        storage_   = std::move(new_storage);
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

}   // namespace detail

template < class Executor = any_io_executor >
struct basic_async_semaphore;

struct semaphore_wait_op_concept;

struct async_semaphore_base
{
    async_semaphore_base(int initial_count);
    async_semaphore_base(async_semaphore_base const &) = delete;
    async_semaphore_base &
    operator=(async_semaphore_base const &)       = delete;
    async_semaphore_base(async_semaphore_base &&) = default;
    async_semaphore_base &
    operator=(async_semaphore_base &&) = default;
    ~async_semaphore_base();

    bool
    try_acquire();

    void
    release();

  protected:
    void
    add_waiter(semaphore_wait_op_concept *waiter);

    int
    decrement();

    [[nodiscard]] int
    count() const noexcept
    {
        return count_;
    }

  private:
    detail::expanding_circular_buffer< semaphore_wait_op_concept * > waiters_;
    int                                                              count_;
};

struct semaphore_wait_op_concept
{
    semaphore_wait_op_concept(async_semaphore_base *host)
    : host_(host)
    {
    }

    virtual void complete(std::error_code) = 0;

    async_semaphore_base *host_;
};

async_semaphore_base::async_semaphore_base(int initial_count)
: waiters_()
, count_(initial_count)
{
}

async_semaphore_base::~async_semaphore_base()
{
    while (waiters_.size())
        waiters_.pop()->complete(error::operation_aborted);
}

void
async_semaphore_base::add_waiter(semaphore_wait_op_concept *waiter)
{
    waiters_.push(waiter);
}

void
async_semaphore_base::release()
{
    count_ += 1;

    // release a pending operations
    if (!waiters_.size())
        return;

    auto next = waiters_.pop();
    decrement();
    next->complete(std::error_code());
}

bool
async_semaphore_base::try_acquire()
{
    bool acquired = false;
    if (count_ > 0)
    {
        --count_;
        acquired = true;
    }
    return acquired;
}

int
async_semaphore_base::decrement()
{
    assert(count_ > 0);
    return --count_;
}

template < class Executor, class Handler >
struct semaphore_wait_op_model : semaphore_wait_op_concept
{
    using executor_type          = Executor;
    using cancellation_slot_type = associated_cancellation_slot_t< Handler >;
    using allocator_type         = associated_allocator_t< Handler >;

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
        return work_guard_.get_executor();
    }

    static semaphore_wait_op_model *
    construct(async_semaphore_base *host, Executor e, Handler handler);

    static void
    destroy(semaphore_wait_op_model *self);

    semaphore_wait_op_model(async_semaphore_base *host,
                            Executor              e,
                            Handler               handler)
    : semaphore_wait_op_concept(host)
    , work_guard_(std::move(e))
    , handler_(std::move(handler))
    {
    }

    virtual void
    complete(error_code ec) override
    {
        auto e = get_executor();
        auto h = std::move(handler_);
        destroy(this);
        post(e, experimental::append(std::move(h), ec));
    }

    executor_work_guard< Executor > work_guard_;
    Handler                         handler_;
};

template < class Executor, class Handler >
auto
semaphore_wait_op_model< Executor, Handler >::construct(
    async_semaphore_base *host,
    Executor              e,
    Handler               handler) -> semaphore_wait_op_model *
{
    auto halloc = get_associated_allocator(handler);
    auto alloc  = typename std::allocator_traits< decltype(halloc) >::
        template rebind_alloc< semaphore_wait_op_model >(halloc);
    auto traits = std::allocator_traits< decltype(alloc) >();
    auto pmem   = traits.allocate(alloc, sizeof(semaphore_wait_op_model));
    try
    {
        return std::construct_at(static_cast< semaphore_wait_op_model * >(pmem),
                                 host,
                                 std::move(e),
                                 std::move(handler));
    }
    catch (...)
    {
        traits.deallocate(alloc, pmem, sizeof(semaphore_wait_op_model));
        throw;
    }
}

template < class Executor, class Handler >
auto
semaphore_wait_op_model< Executor, Handler >::destroy(
    semaphore_wait_op_model *self) -> void
{
    auto halloc = self->get_allocator();
    auto alloc  = typename std::allocator_traits< decltype(halloc) >::
        template rebind_alloc< semaphore_wait_op_model >(halloc);
    std::destroy_at(self);
    auto traits = std::allocator_traits< decltype(alloc) >();
    traits.deallocate(alloc, self, sizeof(semaphore_wait_op_model));
}

template < class Executor >
struct basic_async_semaphore : async_semaphore_base
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
    : async_semaphore_base(initial_count)
    , exec_(std::move(exec))
    {
    }

    executor_type const &
    get_executor() const
    {
        return exec_;
    }

    template < ASIO_COMPLETION_TOKEN_FOR(void(std::error_code))
                   CompletionHandler ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(
                       executor_type) >
    ASIO_INITFN_RESULT_TYPE(CompletionHandler, void(std::error_code))
    async_acquire(
        CompletionHandler &&token ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return async_initiate< CompletionHandler, void(std::error_code) >(
            [this]< class Handler >(Handler &&handler)
            {
                auto e = get_associated_executor(handler, get_executor());
                if (count())
                {
                    post(std::move(e),
                         experimental::append(std::forward< Handler >(handler),
                                              std::error_code()));
                    return;
                }

                using handler_type = std::decay_t< Handler >;
                using model_type =
                    semaphore_wait_op_model< decltype(e), handler_type >;
                model_type *model = model_type ::construct(
                    this, std::move(e), std::forward< Handler >(handler));
                try
                {
                    add_waiter(model);
                }
                catch (...)
                {
                    model_type::destroy(model);
                    throw;
                }
            },
            token);
    }

    executor_type exec_;
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
