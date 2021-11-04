//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_BASIC_TRANSFER_LATCH_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_BASIC_TRANSFER_LATCH_HPP

#include "concepts/transfer_latch.hpp"

namespace asioex
{
/// @brief A latch that satisfies the concept concepts::transfer_latch.
///
/// An object of this type represents the the ability to lock and commit state for a value transfer.
/// @tparam Mutex is a type that satisfies concepts::basic_lockable. Common
///         models would be std::mutex and null_mutex
///
template < concepts::basic_lockable Mutex >
class basic_transfer_latch
{
    bool                        transferred_ = false;
    [[no_unique_address]] Mutex mutex_;

  public:
    using mutex_type = Mutex;

    mutex_type &
    mutex() noexcept;

    /// @brief Test whether this latch has yet to be committed
    /// @pre The mutex must be locked
    /// @return
    bool
    may_commit() const noexcept;

    /// @brief Commit the transfer.
    /// @pre mutex is locked
    /// @pre may_commit() == true
    /// @post may_commit() == false
    /// @post the mutex is unlocked
    void
    commit() noexcept;

    /// @brief Commit the transfer.
    /// @pre mutex is locked
    /// @pre may_commit() == true
    /// @post may_commit() == true
    /// @post the mutex is unlocked
    void
    rollback() noexcept;
};

template < concepts::basic_lockable Mutex >
void
basic_transfer_latch< Mutex >::commit() noexcept
{
    assert(!transferred_);
    transferred_ = true;
    mutex_.unlock();
};

template < concepts::basic_lockable Mutex >
bool
basic_transfer_latch< Mutex >::may_commit() const noexcept
{
    return !transferred_;
}

template < concepts::basic_lockable Mutex >
auto
basic_transfer_latch< Mutex >::mutex() noexcept -> mutex_type &
{
    return mutex_;
}

template < concepts::basic_lockable Mutex >
void
basic_transfer_latch< Mutex >::rollback() noexcept
{
    assert(!transferred_);
    mutex_.unlock();
}

}   // namespace asioex

#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_BASIC_TRANSFER_LATCH_HPP
