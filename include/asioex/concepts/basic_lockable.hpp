//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPT_BASIC_LOCKABLE_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPT_BASIC_LOCKABLE_HPP

#include <concepts>

namespace asioex::concepts
{
/// @brief Describes the standard library concept BasicLockable.
/// @see https://en.cppreference.com/w/cpp/named_req/BasicLockable
// clang-format off
template<class T>
concept basic_lockable = requires(T& m)
{
    { m.lock() };
    { m.unlock() };
    { m.try_lock() } -> std::convertible_to<bool>;
};
// clang-format on

}

#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPT_BASIC_LOCKABLE_HPP
