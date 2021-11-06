//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/router
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_TRANSACTION_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_TRANSACTION_HPP

#include <asioex/concepts/transfer_latch.hpp>

namespace asioex
{
/// @brief Describe a transaction involving one transfer latch
/// @tparam Latch
template < concepts::transfer_latch Latch >
class transaction1
{
    Latch *latch_;

  public:
    explicit transaction1(Latch *latch = nullptr) noexcept;

    transaction1(transaction1 const &) = delete;

    transaction1 &
    operator=(transaction1 const &) = delete;

    bool
    may_commit() const noexcept;

    void
    rollback() noexcept;

    void
    commit() noexcept;

    ~transaction1();
};

/// @brief Describe a transaction involving two transfer latches
/// @tparam Latch
template < concepts::transfer_latch Latch >
struct transaction2
{
    Latch *latch1_ = nullptr;
    Latch *latch2_ = nullptr;

  public:
    explicit transaction2(Latch *latch1, Latch *latch2) noexcept;

    explicit transaction2() noexcept;

    transaction2(transaction2 const &) = delete;

    transaction2 &
    operator=(transaction2 const &) = delete;

    bool
    may_commit() const noexcept;

    void
    rollback() noexcept;

    void
    commit() noexcept;

    ~transaction2();
};

/// @brief Begin a transaction involving one transfer latch
/// @tparam Latch
template < concepts::transfer_latch Latch >
transaction1< Latch >
begin_transaction(Latch &latch) noexcept;

/// @brief Begin a transaction involving two transfer latches
/// @tparam Latch
template < concepts::transfer_latch Latch >
transaction2< Latch >
begin_transaction(Latch &latch1, Latch &latch2) noexcept;

}   // namespace asioex


#include <asioex/impl/transaction.hpp>

#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_TRANSACTION_HPP
