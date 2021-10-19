//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTAL_DETAIL_SEMAPHORE_WAIT_OP_MODEL_HPP
#define ASIO_EXPERIMENTAL_DETAIL_SEMAPHORE_WAIT_OP_MODEL_HPP

#include <asio/associated_allocator.hpp>
#include <asio/associated_cancellation_slot.hpp>
#include <asio/error_code.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/experimental/detail/semaphore_wait_op.hpp>

namespace asio
{
namespace experimental
{
namespace detail
{
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
                            Handler               handler);

    virtual void
    complete(error_code ec) override;

  private:
    executor_work_guard< Executor > work_guard_;
    Handler                         handler_;
};

}   // namespace detail
}   // namespace experimental
}   // namespace asio

#endif

#include <asio/experimental/detail/impl/semaphore_wait_op_model.hpp>
