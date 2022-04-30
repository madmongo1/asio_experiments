// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <asioex/bind_lifetime.hpp>
#include <asio.hpp>


int main(int argc, char * argv[])
{
    std::weak_ptr<int> lt;

    asio::io_context ctx;

    {
        auto p = std::make_shared<int>();
        lt = p;
        asio::post(ctx, asioex::bind_lifetime(
                            p,
                            [&]{
                                assert(!lt.expired());
                            }
                            ));
        assert(!lt.expired());
    }

    assert(!lt.expired());
    ctx.run();
    assert(lt.expired());

    return 0;
}