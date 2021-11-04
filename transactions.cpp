//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#include <asioex/mt/transfer_latch.hpp>
#include <asioex/st/transfer_latch.hpp>
#include <asioex/transaction.hpp>

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

int
main()
{
    test1();
    test2();
}
