// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_FOR_HPP
#define ASIO_EXPERIMENTS_FOR_HPP

#include <asioex/redirect_cancellation.hpp>
#include <asio/experimental/basic_channel.hpp>
#include <asio/experimental/coro.hpp>
#include <asio/use_awaitable.hpp>

namespace asioex
{

template<typename ...Ts>
struct range_from_channel;

template<typename Executor, typename Traits, typename Error, typename T>
struct range_from_channel<Executor, Traits, void(Error, T)>
{
    asio::experimental::basic_channel<Executor, Traits, void(Error, T)> & chan;


    template<typename Exec>
    asio::awaitable<bool, Exec> wait(asio::use_awaitable_t<Exec> tk)
    {
        const auto st = co_await asio::this_coro::cancellation_state;
        if (st.cancelled() != asio::cancellation_type::none
               || !chan.is_open())
            co_return false;

        if (has_value)
            init() = co_await chan.async_receive(tk);
        else
        {
            has_value = true;
            new (&storage_) T(co_await chan.async_receive(tk));
        }
        co_return true;
    }

    T& init()
    {
        return *reinterpret_cast<T*>(&storage_);
    }

    std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
    bool has_value;

    ~range_from_channel()
    {
        if (has_value)
            init().~T();
    }

};

template<typename ...Ts>
auto range_from(asio::experimental::basic_channel<Ts...> & chan)
{
    return range_from_channel<Ts...>{chan};
}



template<typename Yield, typename Executor>
struct range_from_coro
{
    asio::experimental::coro<Yield, void, Executor> & coro;
    using value_type = typename asio::experimental::coro<Yield, void, Executor>::yield_type;

    template<typename Exec>
    asio::awaitable<bool, Exec> wait(asio::use_awaitable_t<Exec> tk)
    {
        const auto st = co_await asio::this_coro::cancellation_state;
        if (st.cancelled() != asio::cancellation_type::none || !coro.is_open())
            co_return false;

        auto value = co_await coro.async_resume(tk);
        if (!value)
            co_return false;

        if (has_value)
            init() = std::move(*value);
        else
        {
            has_value = true;
            new (&storage_) value_type(std::move(*value));
        }

        co_return true;
    }

    value_type& init()
    {
        return *reinterpret_cast<value_type*>(&storage_);
    }

    std::aligned_storage_t<sizeof(value_type), alignof(value_type)> storage_;
    bool has_value;

    ~range_from_coro()
    {
        if (has_value)
            init().~value_type();
    }

};

template<typename Yield, typename Executor>
auto range_from(asio::experimental::coro<Yield, void, Executor> & coro)
{
    return range_from_coro<Yield, Executor>{coro};
}

#define co_for(Var, Src, Token) \
    if (auto r = asioex::range_from(Src); true)   \
        for (auto & Var = r.init(); \
             co_await r.wait(Token);)
}





#endif   // ASIO_EXPERIMENTS_FOR_HPP
