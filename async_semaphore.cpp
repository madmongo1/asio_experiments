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
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>

#include <iostream>
#include <random>

namespace asio::experimental
{
namespace detail
{
struct bilist_node
{
    bilist_node()
    : next_(this)
    , prev_(this)
    {
    }

    bilist_node(bilist_node const &) = delete;
    bilist_node &
    operator=(bilist_node const &) = delete;
    ~bilist_node()                 = default;

    void
    unlink() noexcept
    {
        auto p   = prev_;
        auto n   = next_;
        n->prev_ = p;
        p->next_ = n;
    }

    void
    link_before(bilist_node *next)
    {
        next_        = next;
        prev_        = next->prev_;
        prev_->next_ = this;
        next->prev_  = this;
    }

    bilist_node *next_;
    bilist_node *prev_;
};

}   // namespace detail
namespace st
{
template < class Executor = any_io_executor >
struct basic_async_semaphore;

struct semaphore_wait_op;

struct async_semaphore_base
{
    async_semaphore_base(int initial_count);
    async_semaphore_base(async_semaphore_base const &) = delete;
    async_semaphore_base &
    operator=(async_semaphore_base const &)       = delete;
    async_semaphore_base(async_semaphore_base &&) = delete;
    async_semaphore_base &
    operator=(async_semaphore_base &&) = delete;
    ~async_semaphore_base();

    bool
    try_acquire();

    void
    release();

  protected:
    void
    add_waiter(semaphore_wait_op *waiter);

    int
    decrement();

    [[nodiscard]] int
    count() const noexcept
    {
        return count_;
    }

  private:
    detail::bilist_node waiters_;
    int                 count_;
};

struct semaphore_wait_op : detail::bilist_node
{
    semaphore_wait_op(async_semaphore_base *host)
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
    detail::bilist_node *p = &waiters_;
    while (p->next_ != &waiters_)
    {
        detail::bilist_node *current = p;
        p                            = p->next_;
        static_cast< semaphore_wait_op * >(current)->complete(
            error::operation_aborted);
    }
}

void
async_semaphore_base::add_waiter(semaphore_wait_op *waiter)
{
    waiter->link_before(&waiters_);
}

void
async_semaphore_base::release()
{
    count_ += 1;

    // release a pending operations
    if (waiters_.next_ == &waiters_)
        return;

    decrement();
    static_cast< semaphore_wait_op * >(waiters_.next_)
        ->complete(std::error_code());
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
struct semaphore_wait_op_model final : semaphore_wait_op
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
    : semaphore_wait_op(host)
    , work_guard_(std::move(e))
    , handler_(std::move(handler))
    {
        auto slot = get_cancellation_slot();
        if (slot.is_connected())
            slot.assign(
                [this](cancellation_type)
                {
                    semaphore_wait_op_model *self = this;
                    self->get_cancellation_slot().clear();
                    self->complete(error::operation_aborted);
                });
    }

    virtual void
    complete(error_code ec) override
    {
        auto g = std::move(work_guard_);
        auto h = std::move(handler_);
        unlink();
        destroy(this);
        post(g.get_executor(), experimental::append(std::move(h), ec));
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
    auto pmem   = traits.allocate(alloc, 1);
    try
    {
        return std::construct_at(static_cast< semaphore_wait_op_model * >(pmem),
                                 host,
                                 std::move(e),
                                 std::move(handler));
    }
    catch (...)
    {
        traits.deallocate(alloc, pmem, 1);
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
    traits.deallocate(alloc, self, 1);
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
                    (post)(
                        std::move(e),
                        (experimental::append)(std::forward< Handler >(handler),
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
using namespace experimental::awaitable_operators;

awaitable< void >
co_sleep(std::chrono::milliseconds ms)
{
    auto t = use_awaitable.as_default_on(
        steady_timer(co_await this_coro::executor, ms));
    co_await t.async_wait();
}

awaitable< void >
timeout(std::chrono::milliseconds ms)
{
    auto t = steady_timer(co_await this_coro::executor, ms);
    co_await t.async_wait(as_tuple(use_awaitable));
}

awaitable< void >
bot(int n, st::async_semaphore &sem, std::chrono::milliseconds deadline)
{
    auto say = [ident = "bot " + std::to_string(n) + " : "](auto &&...args)
    {
        std::cout << ident;
        ((std::cout << args), ...);
    };

    if (say("approaching semaphore\n"); !sem.try_acquire())
    {
        say("waiting up to ", deadline.count(), "ms\n");
        auto then = std::chrono::steady_clock::now();
        if ((co_await(sem.async_acquire(as_tuple(use_awaitable)) ||
                      timeout(deadline)))
                .index() == 1)
        {
            say("got bored waiting after ", deadline.count(), "ms\n");
            co_return;
        }
        else
        {
            say("semaphore acquired after ",
                std::chrono::duration_cast< std::chrono::milliseconds >(
                    std::chrono::steady_clock::now() - then)
                    .count(),
                "ms\n");
        }
    }
    else
    {
        say("semaphore acquired immediately\n");
    }

    co_await co_sleep(500ms);
    say("work done\n");

    sem.release();
    say("passed semaphore\n");
}

int
main()
{
    auto ioc  = asio::io_context(ASIO_CONCURRENCY_HINT_UNSAFE);
    auto sem  = st::async_semaphore(ioc.get_executor(), 10);
    auto rng  = std::random_device();
    auto ss   = std::seed_seq { rng(), rng(), rng(), rng(), rng() };
    auto eng  = std::default_random_engine(ss);
    auto dist = std::uniform_int_distribution< unsigned int >(1000, 10000);

    auto random_time = [&eng, &dist]
    { return std::chrono::milliseconds(dist(eng)); };
    for (int i = 0; i < 100; i += 2)
        co_spawn(ioc, bot(i, sem, random_time()), detached);
    ioc.run();
}
