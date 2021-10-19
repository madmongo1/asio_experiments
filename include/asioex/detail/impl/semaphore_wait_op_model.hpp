//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTAL_DETAIL_IMPL_SEMAPHORE_WAIT_OP_MODEL_HPP
#define ASIO_EXPERIMENTAL_DETAIL_IMPL_SEMAPHORE_WAIT_OP_MODEL_HPP

#include <asio/experimental/append.hpp>
#include <asio/post.hpp>
#include <asioex/detail/semaphore_wait_op_model.hpp>

namespace asioex
{
namespace detail
{
template < class Executor, class Handler >
semaphore_wait_op_model< Executor, Handler > *
semaphore_wait_op_model< Executor, Handler >::construct(
    async_semaphore_base *host,
    Executor              e,
    Handler               handler)
{
    auto halloc = get_associated_allocator(handler);
    auto alloc  = typename std::allocator_traits< decltype(halloc) >::
        template rebind_alloc< semaphore_wait_op_model >(halloc);
    auto traits = std::allocator_traits< decltype(alloc) >();
    auto pmem   = traits.allocate(alloc, 1);
    try
    {
        return new (pmem)
            semaphore_wait_op_model(host, std::move(e), std::move(handler));
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

template < class Executor, class Handler >
semaphore_wait_op_model< Executor, Handler >::semaphore_wait_op_model(
    async_semaphore_base *host,
    Executor              e,
    Handler               handler)
: semaphore_wait_op(host)
, work_guard_(std::move(e))
, handler_(std::move(handler))
{
    auto slot = get_cancellation_slot();
    if (slot.is_connected())
        slot.assign(
            [this](asio::cancellation_type type)
            {
                if (!!(type & (asio::cancellation_type::terminal |
                               asio::cancellation_type::partial |
                               asio::cancellation_type::total)))
                {
                    semaphore_wait_op_model *self = this;
                    self->complete(asio::error::operation_aborted);
                }
            });
}

template < class Executor, class Handler >
void
semaphore_wait_op_model< Executor, Handler >::complete(error_code ec)
{
    get_cancellation_slot().clear();
    auto g = std::move(work_guard_);
    auto h = std::move(handler_);
    unlink();
    destroy(this);
    asio::post(g.get_executor(), asio::experimental::append(std::move(h), ec));
}

}   // namespace detail
}   // namespace asioex
#endif
