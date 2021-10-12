//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/blog-2021-10
//

#include <asio.hpp>
#include <asio/experimental/append.hpp>
#include <asio/experimental/prepend.hpp>

#include <iostream>

struct object
{
};

struct my_handler
{
  void operator()(std::error_code)
  {
    std::cout << "(error_code)\n";
  }

  void operator()(std::error_code, std::size_t)
  {
    std::cout << "(error_code, size_t)\n";
  }

  void operator()(std::error_code, std::size_t, std::string)
  {
    std::cout << "(error_code, size_t, string)\n";
  }

  void operator()(std::error_code, std::string)
  {
    std::cout << "(error_code, string)\n";
  }

  void operator()(object*, std::error_code, std::size_t, std::string)
  {
    std::cout << "(object*, error_code, size_t, string)\n";
  }

  void operator()(object*, std::error_code, std::size_t)
  {
    std::cout << "(object*, error_code, size_t)\n";
  }

  void operator()(object*, std::error_code, std::string)
  {
    std::cout << "(object*, error_code, string)\n";
  }

  void operator()(object*, std::error_code)
  {
    std::cout << "(object*, error_code)\n";
  }
};

int main()
{
  using asio::experimental::append;
  using asio::experimental::prepend;

  asio::io_context ioc;
  auto sock = asio::ip::tcp::socket(ioc);
  auto timer = asio::steady_timer(ioc);
  char buf[1024];

  auto handler = my_handler();
  auto self = object();
  auto message = "Hello, World!";

  sock.async_read_some(asio::buffer(buf), handler);
  sock.async_read_some(asio::buffer(buf), prepend(handler, &self));
  sock.async_read_some(asio::buffer(buf), append(prepend(handler, &self), message));

  timer.async_wait(handler);
  timer.async_wait(append(handler, message));
  timer.async_wait(prepend(append(handler, message), &self));
  ioc.run();
}