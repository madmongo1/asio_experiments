//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <asio.hpp>

#include <concepts>
#include <mutex>
#include <optional>

namespace asioex
{
// clang-format off
template<class Mutex>
concept basic_lockable = requires(Mutex& m)
{
    { m.lock() };
    { m.unlock() };
    { m.try_lock() } -> std::convertible_to<bool>;
};
// clang-format on

struct null_mutex
{
    void
    lock();
    void
    unlock();
    bool
    try_lock();
};
static_assert(basic_lockable< null_mutex >);
static_assert(basic_lockable< std::mutex >);

template < basic_lockable Mutex = std::mutex >
class basic_transfer_latch
{
    bool                        transferred_ = false;
    [[no_unique_address]] Mutex mutex_;

  public:
    using mutex_type = Mutex;

    mutex_type &
    mutex() noexcept
    {
        return mutex_;
    }

    bool
    may_commit() const noexcept
    {
        return !transferred_;
    }

    void
    commit() noexcept
    {
        assert(!transferred_);
        transferred_ = true;
        mutex_.unlock();
    };

    void
    rollback() noexcept
    {
        assert(!transferred_);
        mutex_.unlock();
    }

  private:
};

// clang-format off
template<class Latch>
concept transfer_latch = requires(Latch& l, Latch const& cl)
{
    { l.mutex() } -> std::same_as<typename Latch::mutex_type&>;
    { cl.may_commit() } -> std::convertible_to<bool>;
    { l.commit() };
    { l.rollback() };
};
// clang-format on

static_assert(transfer_latch< basic_transfer_latch< std::mutex > >);
static_assert(transfer_latch< basic_transfer_latch< null_mutex > >);

template < transfer_latch Latch >
class transaction1
{
    Latch *latch_;

  public:
    explicit transaction1(Latch *latch = nullptr) noexcept
    : latch_(latch)
    {
    }

    transaction1(transaction1 const &) = delete;
    transaction1 &
    operator=(transaction1 const &) = delete;

    bool may_commit() const noexcept
    {
        return latch_ != nullptr;
    }

    void
    rollback() noexcept
    {
        if (auto l = std::exchange(latch_, nullptr))
            l->rollback();
    }

    void
    commit() noexcept
    {
        assert(latch_);
        assert(latch_->may_commit());
        std::exchange(latch_, nullptr)->commit();
    }

    ~transaction1() { rollback(); }
};

template < transfer_latch Latch >
struct transaction2
{
    Latch *latch1_ = nullptr;
    Latch *latch2_ = nullptr;

  public:
    explicit transaction2(Latch *latch1, Latch *latch2) noexcept
    : latch1_(latch1)
    , latch2_(latch2)
    {
        assert(latch1_ && latch2_);
    }

    explicit transaction2() noexcept
    : latch1_(nullptr)
    , latch2_(nullptr)
    {
    }

    transaction2(transaction2 const &) = delete;

    transaction2 &
    operator=(transaction2 const &) = delete;

    bool
    may_commit() const noexcept
    {
        return latch1_ != nullptr;
    }

    void
    rollback() noexcept
    {
        if (auto l1 = std::exchange(latch1_, nullptr))
        {
            l1->rollback();
            std::exchange(latch2_, nullptr)->rollback();
        }
    }

    void
    commit() noexcept
    {
        assert(latch1_);
        assert(latch1_->may_commit());
        assert(latch2_);
        assert(latch2_->may_commit());
        std::exchange(latch1_, nullptr)->commit();
        std::exchange(latch2_, nullptr)->commit();
    }

    ~transaction2() { rollback(); }
};

template < transfer_latch Latch >
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

template < basic_lockable LockType >
transaction2< basic_transfer_latch< LockType > >
begin_transaction(basic_transfer_latch< LockType > &latch1,
                  basic_transfer_latch< LockType > &latch2) noexcept
{
    std::lock(latch1.mutex(), latch2.mutex());

    if (latch1.may_commit() && latch2.may_commit())
    {
        return transaction2< basic_transfer_latch< LockType > >(&latch1,
                                                                &latch2);
    }
    latch1.mutex().unlock();
    latch2.mutex().unlock();
    return transaction2< basic_transfer_latch< LockType > >();
}

}   // namespace asioex

void test2()
{
    asioex::basic_transfer_latch<> l1, l2;

    assert(l1.may_commit());
    assert(l2.may_commit());

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        t1.rollback();
    }

    assert(l1.may_commit());
    assert(l2.may_commit());

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        assert(l1.may_commit());
        assert(l2.may_commit());
        t1.commit();
        assert(!l1.may_commit());
        assert(!l2.may_commit());
    }

    assert(!l1.may_commit());
    assert(!l2.may_commit());

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        assert(!l1.may_commit());
        assert(!l2.may_commit());
        assert(!t1.may_commit());
    }
}

void test1()
{
        asioex::basic_transfer_latch<> l1;

    assert(l1.may_commit());

    {
        auto t1 = asioex::begin_transaction(l1);
        t1.rollback();
    }

    assert(l1.may_commit());

    {
        auto t1 = asioex::begin_transaction(l1);
        assert(l1.may_commit());
        t1.commit();
        assert(!l1.may_commit());
    }

    assert(!l1.may_commit());

    {
        auto t1 = asioex::begin_transaction(l1);
        assert(!l1.may_commit());
        assert(!t1.may_commit());
    }

}

int
main()
{
    test1();
    test2();
}
