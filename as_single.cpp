//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/blog-2021-10
//

#include <asio.hpp>
#include <asio/experimental/as_single.hpp>

#include <iostream>


int main()
{
  asio::io_context ioc;
  auto sock = asio::ip::tcp::socket(ioc);
  auto timer = asio::steady_timer(ioc);
  char buf[1024];

  sock.async_read_some(asio::buffer(buf), [](std::error_code, std::size_t){});
  sock.async_read_some(asio::buffer(buf), asio::experimental::as_single([](std::tuple<std::error_code, std::size_t>){}));

  timer.async_wait([](std::error_code){});
  timer.async_wait(asio::experimental::as_single([](std::error_code){}));
  ioc.run();
}