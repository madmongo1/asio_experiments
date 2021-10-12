//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/blog-2021-10
//

#ifndef BLOG_OCT_2021_DEBUG_REPORT_FATAL_HPP
#define BLOG_OCT_2021_DEBUG_REPORT_FATAL_HPP

#include <iostream>

namespace debug
{
template<class Context>
struct fatal_reporter
{
    Context context;

    void operator()(std::exception_ptr ep, auto...args) const
    {
      std::cout << context << ": ";
      try {
        if (ep)
          std::rethrow_exception(ep);
        const char* sep = "";
        auto emit = [&sep](auto&& arg)
        {
          std::cout << sep << arg;
          sep = ", ";
        };
        (emit(args),...);
      }
      catch(std::exception& e)
      {
        std::cout << "exception: " << e.what() << "\n";
      }
      std::cout << "\n";
    }
};

inline constexpr auto report_fatal(auto context)
{
  return fatal_reporter<decltype(context)> { context };
}

}

#endif // BLOG_OCT_2021_DEBUG_REPORT_FATAL_HPP
