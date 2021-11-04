//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_MT_TRANSFER_LATCH_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_MT_TRANSFER_LATCH_HPP

#include <asioex/basic_transfer_latch.hpp>

#include <mutex>

namespace asioex::mt
{
using transfer_latch = basic_transfer_latch< std::mutex >;

}
#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_MT_TRANSFER_LATCH_HPP
