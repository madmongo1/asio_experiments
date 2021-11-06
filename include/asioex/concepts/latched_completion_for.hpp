//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPTS_LATCHED_COMPLETION_FOR_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPTS_LATCHED_COMPLETION_FOR_HPP

#include <asio/async_result.hpp>

namespace asioex::concepts
{
// clang-format off
template<class T, typename...Signatures>
concept latched_completion_for =
requires(T& lc)
{
    { lc.latch };
    {
        asio::async_initiate<decltype(lc.token), Signatures...>
        (
            asio::detail::initiation_archetype<Signatures...>{},
            lc.token
        )
    };
};
// clang-format on

}   // namespace asioex::concepts
#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPTS_LATCHED_COMPLETION_FOR_HPP
