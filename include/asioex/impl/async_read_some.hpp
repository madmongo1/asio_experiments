//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/router
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_IMPL_ASYNC_READ_SOME_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_IMPL_ASYNC_READ_SOME_HPP

#include <asio/compose.hpp>
#include <asio/coroutine.hpp>
#include <asio/socket_base.hpp>
#include <asioex/async_read_some.hpp>
#include <asioex/concepts/transfer_latch.hpp>
#include <asioex/error.hpp>

namespace asioex
{
template < class Socket,
           class MutableBufferSequence,
           asioex::concepts::transfer_latch Latch >
struct atomic_read_op : asio::coroutine
{
    Socket               &sock;
    MutableBufferSequence buf;
    Latch                &latch;

    Latch &
    get_transfer_latch() const
    {
        return latch;
    }

#include <asio/yield.hpp>
    template < class Self >
    void
    operator()(Self &&self, asioex::error_code ec = {}, std::size_t size = 0)
    {
        reenter(this) for (;;)
        {
            yield sock.async_wait(asio::socket_base::wait_type::wait_read,
                                  std::move(self));

            auto trans = begin_transaction(latch);
            if (!trans.may_commit())
            {
                trans.rollback();
                return self.complete(error::completion_denied, 0);
            }
            if (ec)
            {
                trans.commit();
                return self.complete(ec, size);
            }

            auto wasblocked = sock.non_blocking();
            sock.non_blocking(true);
            size = sock.read_some(buf, ec);
            sock.non_blocking(wasblocked);
            if (ec == asio::error::would_block)
            {
                ec.clear();
                trans.rollback();
                continue;
            }
            trans.commit();
            return self.complete(ec, size);
        }
    }
#include <asio/unyield.hpp>
};

template < class Socket,
           class MutableBufferSequence,
           concepts::latched_completion_for<
               void(std::error_code, std::size_t) > LatchedCompletion >
auto
async_read_some(Socket &sock, MutableBufferSequence buf, LatchedCompletion lc)
    -> ASIO_INITFN_RESULT_TYPE(decltype(lc.token),
                               void(std::error_code, std::size_t))
{
    return asio::async_compose< decltype(lc.token),
                                void(std::error_code, std::size_t) >(
        atomic_read_op< Socket,
                        MutableBufferSequence,
                        std::remove_reference_t< decltype(lc.latch) > > {
            .sock = sock, .buf = buf, .latch = lc.latch },
        lc.token,
        sock);
}

}   // namespace asioex

#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_IMPL_ASYNC_READ_SOME_HPP
