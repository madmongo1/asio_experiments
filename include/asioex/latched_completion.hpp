//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_LATCHED_COMPLETION_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_LATCHED_COMPLETION_HPP

#include <asioex/concepts/transfer_latch.hpp>
#include <utility>

namespace asioex
{
template < concepts::transfer_latch Latch, class CompletionToken >
struct latched_completion
{
    latched_completion(Latch &latch, CompletionToken token)
    : latch(latch)
    , token(std::move(token))
    {
    }

    Latch          &latch;
    CompletionToken token;
};

template < class Latch, class Token >
latched_completion(Latch &, Token &&)
    -> latched_completion< Latch, std::decay_t< Token > >;

}
#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_LATCHED_COMPLETION_HPP
