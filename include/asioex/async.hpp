// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_ASYNC_HPP
#define ASIO_EXPERIMENTS_ASYNC_HPP

#include <asio/compose.hpp>
#include <asio/experimental/deferred.hpp>
#include <coroutine>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <boost/preprocessor/repeat.hpp>
#include <boost/preprocessor/repeat_2nd.hpp>

namespace asioex
{

template<typename ... Signatures>
struct compose_tag
{
};

namespace detail
{

template<typename Derived, typename Signature>
struct compose_promise_base;

template<typename Derived, typename ... Args>
struct compose_promise_base<Derived, void(Args...)>
{
    void return_value(std::tuple<Args...>  args)
    {
        static_cast<Derived*>(this)->result_ = std::move(args);
    }

    using tuple_type = std::tuple<Args...>;
};

template<typename Tag, typename Token, typename ... Args>
struct compose_promise;



template<typename ...Sigs, typename Token, typename ... Args>
struct compose_promise<compose_tag<Sigs...>, Token, Args...>
    : compose_promise_base<compose_promise<compose_tag<Sigs...>, Token, Args...>, Sigs> ...
{
    using compose_promise_base<compose_promise<compose_tag<Sigs...>, Token, Args...>, Sigs> ::return_value ...;
    using result_type = std::variant<
        typename compose_promise_base<compose_promise<compose_tag<Sigs...>, Token, Args...>, Sigs>
            ::tuple_type ...>;

    result_type result_;
    Token & token;

    // TODO Pick the executor from one of the args similar to compose
    compose_promise(Args & ... args, Token & tk, compose_tag<Sigs...>) : token(tk)
    {
    }

    ~compose_promise()
    {
        std::visit(
            [this](auto & tup)
            {
                std::apply(complete_, std::move(tup));
            }, result_);
    }

    constexpr static std::suspend_never initial_suspend() noexcept { return {}; }
    constexpr static std::suspend_never   final_suspend() noexcept { return {}; }

    template<typename ... Args_, typename ... Ts>
    auto await_transform(asio::experimental::deferred_async_operation<void(Args_...), Ts...> op)
    {

        struct result
        {
            asio::experimental::deferred_async_operation<void(Args_...), Ts...>  op;
            bool await_ready() { return false; }
            void await_suspend( std::coroutine_handle<compose_promise> h)
            {
                std::move(op)(
                    [this, pp = std::unique_ptr<void, coro_delete>(h.address())](Args_... args) mutable
                    {
                        res = {std::move(args)...};
                        std::coroutine_handle<compose_promise>::from_address(pp.release()).resume();
                    });
            }

            std::tuple<Args_...> await_resume()
            {
                return std::move(res);
            }

            std::tuple<Args_...> res;
        };
        return result{std::move(op)};
    };

    struct coro_delete
    {
        void operator()(void * c)
        {
            if (c != nullptr)
                std::coroutine_handle<compose_promise>::from_address(c).destroy();
        }
    };

    auto get_return_object() -> typename asio::async_result<std::decay_t<Token>,
                                                        void(std::error_code, int)>::return_type
    {
        return asio::async_initiate<Token, Sigs...>(
            [this](auto tk)
            {
                complete_ = tk;
            }, token);
    }

    void unhandled_exception()
    {
        // TODO: mangle and throw from executor.
        std::terminate();
    }

    // TODO implement for overloads
    std::function<Sigs...> complete_;
};

}

}

namespace std
{

#define ASIOEX_TYPENAME(z, n, text) , typename T##n
#define ASIOEX_SPEC(z, n, text) , T##n
#define ASIOEX_TRAIT_DECL(z, n, text) \
template<typename Return BOOST_PP_REPEAT_2ND(n, ASIOEX_TYPENAME, ), typename Token, typename ... Sigs > \
struct coroutine_traits<Return BOOST_PP_REPEAT_2ND(n, ASIOEX_SPEC, ), Token, asioex::compose_tag<Sigs...>> \
{  \
    using promise_type = asioex::detail::compose_promise< \
                            asioex::compose_tag<Sigs...>, Token         \
                            BOOST_PP_REPEAT_2ND(n, ASIOEX_SPEC, )>;     \
};

BOOST_PP_REPEAT(24, ASIOEX_TRAIT_DECL, );

#undef ASIOEX_TYPENAME
#undef ASIOEX_SPEC
#undef ASIOEX_TRAIT_DECL

}

#endif   // ASIO_EXPERIMENTS_ASYNC_HPP
