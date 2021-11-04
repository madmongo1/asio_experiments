//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/router
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPT_TRANSFER_LATCH_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPT_TRANSFER_LATCH_HPP

#include <concepts>
#include "basic_lockable.hpp"

namespace asioex::concepts
{
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

};
#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_CONCEPT_TRANSFER_LATCH_HPP
