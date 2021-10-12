//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/blog-2021-10
//

#include <asio.hpp>
#include <asio/experimental/deferred.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>

#include <iostream>

asio::awaitable<void>
writer(asio::ip::tcp::socket sock)
{
  using asio::use_awaitable;

  const std::string_view
  buffers[] = {
    "the", "cat", "sat", "on", "the", "mat"
  };

  std::string compose_buffer;
  for(auto b : buffers)
  {
    compose_buffer.assign(b);
    compose_buffer += '\n';
    co_await asio::async_write(sock, asio::buffer(compose_buffer), use_awaitable);
  }

  // sock will be automatically shut down and closed on destruction
}

asio::awaitable<void>
reader(asio::ip::tcp::socket sock)
{
  using asio::experimental::deferred;
  using asio::use_awaitable;
  using asio::experimental::as_tuple;

  // An easy but not efficient read buffer
  std::string buf;

  // created the deferred operation object
  auto deferred_read = async_read_until(
      sock,
      asio::dynamic_buffer(buf),
      "\n",
      deferred);

  // deferring a deferred operation is a no-op
  auto deferred_read2 = deferred_read(deferred);

  // tokens are objects which can be composed and stored for later
  // The as_tuple token causes the result type to be reported as a
  // tuple where the first element is the error type. This prevents
  // the coroutine from throwing an exception.
  const auto my_token = as_tuple(use_awaitable);

  bool selector = false;
  for(;;)
  {
    // use each deferred operation alternately
    auto [ec, n] = co_await [&] {
      selector = !selector;
      if (!selector)
        return deferred_read(my_token);
      else
        return deferred_read2(my_token);
    }();
    if (ec)
    {
      std::cout << "reader finished: " << ec.message() << "\n";
      break;
    }
    auto view = std::string_view(buf.data(), n - 1);
    std::cout << "reader: " << view << "\n";
    buf.erase(0, n);
  }
}

asio::awaitable<
    std::tuple<
        asio::ip::tcp::socket,
        asio::ip::tcp::socket>>
local_socket_pair()
{
  namespace this_coro = asio::this_coro;
  using namespace asio::experimental::awaitable_operators;
  using asio::use_awaitable;

  auto acceptor =
    asio::ip::tcp::acceptor (
          co_await this_coro::executor,
          asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));

  acceptor.listen();

  auto client = asio::ip::tcp::socket(co_await this_coro::executor);
  auto server = asio::ip::tcp::socket(co_await this_coro::executor);
  co_await (
      acceptor.async_accept(server, use_awaitable) &&
      client.async_connect(acceptor.local_endpoint(), use_awaitable)
  );

  co_return std::make_tuple(std::move(client), std::move(server));
}

asio::awaitable<void>
run()
{
  using namespace asio::experimental::awaitable_operators;

  auto [client, server] = co_await local_socket_pair();

  co_await (
    reader(std::move(server)) &&
    writer(std::move(client))
  );
}

int main()
{
  using asio::experimental::deferred;
  using asio::detached;

  asio::io_context ioc;

  asio::co_spawn(ioc, run(), detached);


  ioc.run();
}