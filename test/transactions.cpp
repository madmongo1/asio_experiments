//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <asio.hpp>

#include <asioex/mt/transfer_latch.hpp>
#include <asioex/st/transfer_latch.hpp>
#include <asioex/transaction.hpp>

#include <forward_list>
#include <queue>

namespace asioex
{
static_assert(concepts::basic_lockable< std::mutex >);
static_assert(concepts::basic_lockable< null_mutex >);
static_assert(concepts::transfer_latch< mt::transfer_latch >);
static_assert(concepts::transfer_latch< st::transfer_latch >);

}   // namespace asioex

void
test2()
{
    asioex::mt::transfer_latch l1, l2;

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        assert(l1.may_commit());
        assert(l2.may_commit());
        t1.rollback();
    }

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        assert(l1.may_commit());
        assert(l2.may_commit());
        t1.commit();
    }

    {
        auto t1 = asioex::begin_transaction(l1, l2);
        assert(!l1.may_commit());
        assert(!l2.may_commit());
        assert(!t1.may_commit());
    }
}

void
test1()
{
    asioex::mt::transfer_latch l1;

    assert(l1.may_commit());

    {
        auto t1 = asioex::begin_transaction(l1);
        assert(l1.may_commit());
        t1.rollback();
    }

    {
        auto t1 = asioex::begin_transaction(l1);
        assert(l1.may_commit());
        t1.commit();
    }

    {
        auto t1 = asioex::begin_transaction(l1);
        assert(!l1.may_commit());
        assert(!t1.may_commit());
    }
}

namespace asioex
{
namespace concepts
{
}   // namespace concepts

namespace concepts
{
// clang-format off
template<class T, class Value>
concept value_source =
requires(T& x)
{
    { x.take() } -> std::convertible_to<T&&>;
};

template<class T, class Value>
concept value_target =
requires(T& x, Value& v)
{
    { x.store(std::move(v)) };
};
// clang-format on
}   // namespace concepts

template < concepts::transfer_latch Latch, class Source, class Target >
requires requires(Source s, Target t)
{
    { t(s()) };
}
bool
atomic_transfer(Latch &source_latch,
                Latch &target_latch,
                Source source,
                Target target)
{
    if (auto t = begin_transaction(source_latch, target_latch); t.may_commit())
    {
        // note: we don't need to hold the lock during the store. The commit
        // only commits the owner of the store, not it contents
        t.commit();
        target(source());
        return true;
    }
    return false;
}

template < concepts::transfer_latch Latch, class Source, class Target >
requires requires(Source s, Target t)
{
    { t(s()) };
}
bool
atomic_transfer(Latch &latch, Source source, Target target)
{
    if (auto t = begin_transaction(latch); t.may_commit())
    {
        // note: we don't need to hold the lock during the store. The commit
        // only commits the owner of the store, not it contents
        t.commit();
        target(source());
        return true;
    }
    return false;
}

/// @brief Conditionally delivers a value to a target through a transfer_latch
/// transaction.
/// @tparam Value
/// @tparam Target
/// @param target_latch
/// @param val A reference to the value to be moved.
/// @param target
/// @post IFF the target_latch was unlatched after being acquired, val will be
/// in the moved-from state. OTHERWISE, val will be unmodified.
/// @return true if delivered.
template < concepts::transfer_latch Latch, class Value, class Target >
bool
atomic_deliver(Latch &target_latch, Value &val, Target &target)
{
    if (auto t = begin_transaction(target_latch); t.may_commit())
    {
        // note: we don't need to hold the lock during the store. The commit
        // only commits the owner of the store, not it contents
        t.commit();
        target.store(std::move(val));
        return true;
    }
    return false;
}

namespace concepts
{
// clang-format off
template<class Container, class Value>
concept channel_container_of =
requires (Container& c, Container const& cc, Value v)
    {
        { cc.size() } -> std::convertible_to<std::size_t>;
        { c.front() } -> std::convertible_to<Value&>;
        { c.push_back(std::move(v))  };
        { c.pop_front() };
    } &&
    std::move_constructible<Container> &&
    std::destructible<Container>;
// clang-format on

}   // namespace concepts

struct void_type
{
};

// clang-format off
struct void_container
{
    std::size_t size() const noexcept { return size_; }
    void_type& front() noexcept { return value_; }
    void push_back(void_type) { ++size_; };
    void pop_front() { --size_; };
  private:
    std::size_t size_ = 0;
    [[no_unique_address]] void_type value_;
};
// clang-format on

template < concepts::transfer_latch Latch, class Value >
struct delivery_model_base
{
    Latch &latch;

    delivery_model_base(delivery_model_base const &) = delete;
    delivery_model_base &
    operator=(delivery_model_base const &) = delete;

    /// Transfer a value after destroying the current object
    virtual void
    transfer(Value &&src) = 0;

    /// Fail the transfer after destroying the current object
    virtual void
    fail(std::error_code ec) = 0;

    virtual ~delivery_model_base() = default;
};

template < concepts::transfer_latch Latch >
struct delivery_model_base< Latch, void >
{
    Latch &latch;

    delivery_model_base(delivery_model_base const &) = delete;
    delivery_model_base &
    operator=(delivery_model_base const &) = delete;

    /// Transfer a value after destroying the current object
    virtual void
    transfer() = 0;

    /// Fail the transfer after destroying the current object
    virtual void
    fail(std::error_code ec) = 0;

    virtual ~delivery_model_base() = default;
};

template < class Value,
           concepts::channel_container_of< Value > Container,
           concepts::transfer_latch                Latch >
struct value_channel_impl
{
    value_channel_impl(std::size_t cap)
    : queue_()
    , capacity_(cap)
    {
        if constexpr (requires { queue_.reserve(capacity_); })
            queue_.reserve(capacity_);
    }

    /// @brief Close the channel to any new deliveries or receives.
    /// After calling this function, all send operations will wait with
    /// asio::error::eof. Receive operations will succeed if they can be
    /// completed immediately due to there being a value in the queue.
    void
    close();

    bool
    try_send(Value &val);

    bool
    try_send(Value &&val);

  private:
    std::error_code
                error_;   //! Set to indicate that the channel has been closed
    Container   queue_;
    std::size_t capacity_;
    std::forward_list< delivery_model_base< Latch, Value > * > deliver_ops_;
};

template < class Value,
           concepts::channel_container_of< Value > Container,
           concepts::transfer_latch                Latch >
void
value_channel_impl< Value, Container, Latch >::close()
{
    error_ = asio::error::eof;
}

template < class Value,
           concepts::channel_container_of< Value > Container,
           concepts::transfer_latch                Latch >
bool
value_channel_impl< Value, Container, Latch >::try_send(Value &&val)
{
    return try_send(val);
}

template < class Value,
           concepts::channel_container_of< Value > Container,
           concepts::transfer_latch                Latch >
bool
value_channel_impl< Value, Container, Latch >::try_send(Value &val)
{
    auto prev    = deliver_ops_.before_begin();
    auto current = deliver_ops_.begin();
    while (current != deliver_ops_.end())
    {
        delivery_model_base< Latch, Value > *this_waiter = *current;

        if (auto t = begin_transaction(this_waiter->latch); t.may_commit())
        {
            t.commit();
            deliver_ops_.erase_after(prev);
            this_waiter->transfer(std::move(val));
            return true;
        }
        prev = current++;
    }

    // We were unable to find a pending deliver op, so now we see if we can
    // store the value

    if (queue_.size() >= capacity_)
        return false;
    queue_.push_back(std::move(val));
    return true;
}

}   // namespace asioex

void
test_string_channel()
{
    asioex::value_channel_impl< std::string,
                                std::deque< std::string >,
                                asioex::st::transfer_latch >
        ch1(1);

    auto sent = ch1.try_send("Hello");
    assert(sent);
    sent = ch1.try_send("World");
    assert(!sent);

    asioex::value_channel_impl< asioex::void_type,
                                asioex::void_container,
                                asioex::st::transfer_latch >
        ch2(1);

    sent = ch2.try_send(asioex::void_type());
    assert(sent);
    sent = ch2.try_send(asioex::void_type());
    assert(!sent);
}

int
main()
{
    test1();
    test2();

    test_string_channel();
}
