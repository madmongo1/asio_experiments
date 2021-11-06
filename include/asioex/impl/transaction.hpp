//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/router
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_IMPL_TRANSACTION_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_IMPL_TRANSACTION_HPP

#include <asioex/transaction.hpp>

#include <cassert>
#include <mutex>

namespace asioex
{
template < concepts::transfer_latch Latch >
transaction2< Latch >::transaction2() noexcept
: latch1_(nullptr)
, latch2_(nullptr)
{
}

template < concepts::transfer_latch Latch >
bool
transaction2< Latch >::may_commit() const noexcept
{
    return latch1_ != nullptr;
}

template < concepts::transfer_latch Latch >
void
transaction2< Latch >::rollback() noexcept
{
    if (auto l1 = std::exchange(latch1_, nullptr))
    {
        l1->rollback();
        std::exchange(latch2_, nullptr)->rollback();
    }
}

template < concepts::transfer_latch Latch >
void
transaction2< Latch >::commit() noexcept
{
    assert(latch1_);
    assert(latch1_->may_commit());
    assert(latch2_);
    assert(latch2_->may_commit());
    std::exchange(latch1_, nullptr)->commit();
    std::exchange(latch2_, nullptr)->commit();
}

template < concepts::transfer_latch Latch >
transaction2< Latch >::transaction2(Latch *latch1, Latch *latch2) noexcept
: latch1_(latch1)
, latch2_(latch2)
{
    assert(latch1_ && latch2_);
}

template < concepts::transfer_latch Latch >
transaction2< Latch >::~transaction2()
{
    rollback();
}

// transaction1

template < concepts::transfer_latch Latch >
transaction1< Latch >::transaction1(Latch *latch) noexcept
: latch_(latch)
{
}

template < concepts::transfer_latch Latch >
bool
transaction1< Latch >::may_commit() const noexcept
{
    return latch_ != nullptr;
}
template < concepts::transfer_latch Latch >
void
transaction1< Latch >::rollback() noexcept
{
    if (auto l = std::exchange(latch_, nullptr))
        l->rollback();
}
template < concepts::transfer_latch Latch >
void
transaction1< Latch >::commit() noexcept
{
    assert(latch_);
    assert(latch_->may_commit());
    std::exchange(latch_, nullptr)->commit();
}
template < concepts::transfer_latch Latch >
transaction1< Latch >::~transaction1()
{
    rollback();
}

// begin_transaction

template < concepts::transfer_latch Latch >
transaction1< Latch >
begin_transaction(Latch &latch) noexcept
{
    latch.mutex().lock();
    if (latch.may_commit())
    {
        return transaction1< Latch >(&latch);
    }
    latch.mutex().unlock();
    return transaction1< Latch >();
}

template < concepts::transfer_latch Latch >
transaction2< Latch >
begin_transaction(Latch &latch1, Latch &latch2) noexcept
{
    std::lock(latch1.mutex(), latch2.mutex());

    if (latch1.may_commit() && latch2.may_commit())
    {
        return transaction2< Latch >(&latch1, &latch2);
    }
    latch1.mutex().unlock();
    latch2.mutex().unlock();
    return transaction2< Latch >();
}

}   // namespace asioex
#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_IMPL_TRANSACTION_HPP
