//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_NULL_MUTEX_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_NULL_MUTEX_HPP

#include <asioex/concepts/basic_lockable.hpp>

namespace asioex
{
/// @brief A type that models basic_lockable but all operations are no-ops.

struct null_mutex
{
    void
    lock()
    {
    }

    void
    unlock()
    {
    }

    bool
    try_lock()
    {
        return true;
    }
};

static_assert(concepts::basic_lockable< null_mutex >);
}   // namespace asioex

#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_NULL_MUTEX_HPP
