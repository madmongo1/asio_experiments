//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTAL_DETAIL_SEMAPHORE_WAIT_OP
#define ASIO_EXPERIMENTAL_DETAIL_SEMAPHORE_WAIT_OP

#include <asio/experimental/detail/bilist_node.hpp>
#include <asio/error_code.hpp>

namespace asio
{
namespace experimental
{
struct async_semaphore_base;

namespace detail
{
struct semaphore_wait_op : detail::bilist_node
{
    semaphore_wait_op(async_semaphore_base *host);

    virtual void complete(error_code) = 0;

    async_semaphore_base *host_;
};

}   // namespace detail
}   // namespace experimental
}   // namespace asio

#endif

#include <asio/experimental/detail/impl/semaphore_wait_op.hpp>