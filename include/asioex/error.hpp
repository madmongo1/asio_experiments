//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/asio_experiments
//

#ifndef ASIO_EXPERIMENTS_INCLUDE_ASIOEX_ERROR_HPP
#define ASIO_EXPERIMENTS_INCLUDE_ASIOEX_ERROR_HPP

#include <system_error>

namespace asioex
{
struct error
{
    enum latch_error
    {
        completion_denied = 1,
    };
};

inline std::error_category const &
select_category()
{
    static const struct : std::error_category
    {
        const char *
        name() const noexcept
        {
            return "select_category";
        }

        std::string
        message(int ev) const
        {
            switch (static_cast< error::latch_error >(ev))
            {
            case error::completion_denied:
                return "Completion denied";

            default:
                return "Invalid code: " + std::to_string(ev);
            }
        }

    } cat;
    return cat;
}

std::error_code
make_error_code(error::latch_error ec)
{
    return std::error_code(static_cast< int >(ec), select_category());
}

}   // namespace asioex

namespace std
{
template <>
struct is_error_code_enum< ::asioex::error::latch_error > : true_type
{
};
}   // namespace std

#endif   // ASIO_EXPERIMENTS_INCLUDE_ASIOEX_ERROR_HPP
