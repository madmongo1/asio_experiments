//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_ASYNC_READ_SOME_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_ASYNC_READ_SOME_HPP

#include <asio/async_result.hpp>
#include <asioex/concepts/latched_completion_for.hpp>
#include <asioex/error_code.hpp>

namespace asioex
{
template < class Socket,
           class MutableBufferSequence,
           concepts::latched_completion_for<
               void(asioex::error_code, std::size_t) > LatchedCompletion >
auto
async_read_some(Socket &sock, MutableBufferSequence buf, LatchedCompletion lc)
    -> ASIO_INITFN_RESULT_TYPE(decltype(lc.token),
                               void(std::error_code, std::size_t));

}

#include <asioex/impl/async_read_some.hpp>

#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_ASYNC_READ_SOME_HPP
