// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_ASYNC_HPP
#define ASIO_EXPERIMENTS_ASYNC_HPP

#include <asio/compose.hpp>

#include <asio/experimental/deferred.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/this_coro.hpp>

#include <coroutine>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <boost/preprocessor/repeat.hpp>
#include <boost/preprocessor/repeat_2nd.hpp>

namespace asio
{

template<typename E>
struct use_awaitable_t;

namespace experimental
{

template<typename E>
struct use_coro_t;

}

}

namespace asioex
{

template < typename... Signatures >
struct compose_tag
{
};

namespace detail
{

template < typename T >
constexpr auto
compose_token_impl(const T *)
{
    return asio::experimental::deferred;
}

template < typename Executor >
constexpr auto
compose_token_impl(const asio::use_awaitable_t< Executor > *)
{
    return asio::experimental::as_tuple(asio::use_awaitable_t< Executor >());
}

template < typename T >
constexpr auto
compose_token_impl(const asio::experimental::use_coro_t< T > *)
{
    return asio::experimental::as_tuple(asio::experimental::use_coro_t< T >());
}

template < template < class Token, class... > class Modifier,
           class Token,
           class... Ts >
constexpr auto
compose_token_impl(
    const Modifier< Token, Ts... > *,
    typename asio::constraint< !std::is_void< Token >::value >::type = 0)
{
    return compose_token_impl(static_cast< const Token * >(nullptr));
}

}

template < typename T >
constexpr auto
compose_token(const T & val)
{
    return detail::compose_token_impl(&val);
}

namespace detail
{

template<typename T>
auto foo(T&&);

template<typename Token>
auto pick_executor(Token && token)
{
    return asio::get_associated_executor(token);
}


template<typename Token,
         typename First,
         typename ... IoObjectsOrExecutors>
auto pick_executor(Token && token,
                   const First & first,
                   IoObjectsOrExecutors && ... io_objects_or_executors)
    -> typename std::enable_if<
        asio::is_executor<First>::value || asio::execution::is_executor<First>::value
        ,First >::type
{
    return first;
}


template<typename Token,
           typename First,
           typename ... IoObjectsOrExecutors>
auto pick_executor(Token && token,
                   First & first,
                   IoObjectsOrExecutors && ... io_objects_or_executors)
    -> typename First::executor_type
{
    return first.get_executor();
}



template<typename Token,
           typename First,
           typename ... IoObjectsOrExecutors>
auto pick_executor(Token && token,
                   First &&,
                   IoObjectsOrExecutors && ... io_objects_or_executors)
{
    return pick_executor(std::forward<Token>(token),
                         std::forward<IoObjectsOrExecutors>(io_objects_or_executors)...);
}



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

template<typename Return, typename Tag, typename Token, typename ... Args>
struct compose_promise;

template<typename Allocator, typename Tag, typename Token, typename ... Args>
struct compose_promise_alloc_base
{
    using allocator_type = Allocator;
    void* operator new(const std::size_t size,
                 Args & ... args, Token & tk,
                 Tag)
    {
        using alloc_type = typename std::allocator_traits<allocator_type>:: template rebind_alloc<unsigned char>;
        alloc_type alloc{asio::get_associated_allocator(tk)};

        const auto align_needed = size % alignof(alloc_type);
        const auto align_offset = align_needed != 0 ? alignof(alloc_type) - align_needed : 0ull;
        const auto alloc_size = size + sizeof(alloc_type) + align_offset;
        const auto raw = std::allocator_traits<alloc_type>::allocate(alloc, alloc_size);
        new (raw + size + align_offset) alloc_type(std::move(alloc));

        return raw;
    }
    void operator delete(void * raw_,
                    std::size_t size)
    {
        using alloc_type = typename std::allocator_traits<allocator_type>:: template rebind_alloc<unsigned char>;
        const auto raw = static_cast<unsigned char *>(raw_);

        const auto align_needed = size % alignof(alloc_type);
        const auto align_offset = align_needed != 0 ? alignof(alloc_type) - align_needed : 0ull;
        const auto alloc_size = size + sizeof(alloc_type) + align_offset;

        auto alloc_p = reinterpret_cast<alloc_type*>(raw + size + align_offset);
        auto alloc = std::move(*alloc_p);
        alloc_p->~alloc_type();

        std::allocator_traits<alloc_type>::deallocate(alloc, raw, alloc_size);
    }
};

template<typename Tag, typename Token, typename ... Args>
struct compose_promise_alloc_base<std::allocator<void>, Tag, Token, Args...>
{
};

template<typename Return, typename ...Sigs, typename Token, typename ... Args>
struct compose_promise<Return, compose_tag<Sigs...>, Token, Args...>
    :
    compose_promise_alloc_base<
        asio::associated_allocator_t<std::decay_t<Token>>, compose_tag<Sigs...>, Token, Args...>,
    compose_promise_base<compose_promise<Return, compose_tag<Sigs...>, Token, Args...>, Sigs> ...
{
    using compose_promise_base<compose_promise<Return, compose_tag<Sigs...>, Token, Args...>, Sigs> ::return_value ...;
    using result_type = std::variant<
        typename compose_promise_base<compose_promise<Return, compose_tag<Sigs...>, Token, Args...>, Sigs>
            ::tuple_type ...>;

    using token_type = std::decay_t<Token>;

    result_type result_;

    token_type token;
    using allocator_type = asio::associated_allocator_t<token_type>;

    asio::cancellation_state state{
        asio::get_associated_cancellation_slot(token),
        asio::enable_terminal_cancellation()
    };

    using executor_type =
        typename asio::prefer_result<
            decltype(pick_executor(std::declval<Token>(), std::declval<Args>()...)),
            asio::execution::outstanding_work_t::tracked_t>::type;

    executor_type executor_;

    // TODO Pick the executor from one of the args similar to compose
    compose_promise(Args & ... args, Token & tk, compose_tag<Sigs...>)
        : token(tk), executor_(
          asio::prefer(
            pick_executor(token, args...),
              asio::execution::outstanding_work.tracked))
    {
    }

    ~compose_promise()
    {
        if (completion)
            std::visit(
                [this](auto & tup)
                {
                    std::apply(std::move(*completion), std::move(tup));
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
            compose_promise * self;
            std::tuple<Args_...> res;

            struct completion
            {
                compose_promise * self;
                std::tuple<Args_...> &result;
                std::unique_ptr<void, coro_delete> coro_handle;

                using cancellation_slot_type = asio::cancellation_slot;
                cancellation_slot_type get_cancellation_slot() const noexcept
                {
                    return self->state.slot();
                }

                using executor_type = typename compose_promise::executor_type;
                executor_type get_executor() const noexcept
                {
                    return self->executor_;
                }

                using allocator_type = typename compose_promise::allocator_type;
                allocator_type get_allocator() const noexcept
                {
                    return asio::get_associated_allocator(self->token);
                }

                void operator()(Args_ ... args)
                {
                    result = {std::move(args)...};
                    std::coroutine_handle<compose_promise>::from_address(coro_handle.release()).resume();
                }
            };

            bool await_ready() { return false; }
            void await_suspend( std::coroutine_handle<compose_promise> h)
            {
                std::move(op)(completion{self, res, {h.address(), coro_delete{}}});
            }

            std::tuple<Args_...> await_resume()
            {
                return std::move(res);
            }

        };
        return result{std::move(op), this};
    };

    struct coro_delete
    {
        void operator()(void * c)
        {
            if (c != nullptr)
                std::coroutine_handle<compose_promise>::from_address(c).destroy();
        }
    };


    auto await_transform(asio::this_coro::executor_t) const
    {
        struct exec_helper
        {
            const executor_type& value;

            constexpr static bool await_ready() noexcept
            {
                return true;
            }

            constexpr static void await_suspend(std::coroutine_handle<>) noexcept
            {
            }

            executor_type await_resume() const noexcept
            {
                return value;
            }
        };

        return exec_helper{executor_};
    }

    auto await_transform(asio::this_coro::cancellation_state_t) const
    {
        struct exec_helper
        {
            const asio::cancellation_state& value;

            constexpr static bool await_ready() noexcept
            {
                return true;
            }

            constexpr static void await_suspend(std::coroutine_handle<>) noexcept
            {
            }

            asio::cancellation_state await_resume() const noexcept
            {
                return value;
            }
        };
        return exec_helper{state};
    }

    // This await transformation resets the associated cancellation state.
    auto await_transform(asio::this_coro::reset_cancellation_state_0_t) noexcept
    {
        struct result
        {
            asio::cancellation_state &state;
            token_type & token;

            bool await_ready() const noexcept
            {
                return true;
            }

            void await_suspend(std::coroutine_handle<void>) noexcept
            {
            }

            auto await_resume() const
            {
                state = asio::cancellation_state(asio::get_associated_cancellation_slot(token));
            }
        };

        return result{state, token};
    }

    // This await transformation resets the associated cancellation state.
    template <typename Filter>
    auto await_transform(
        asio::this_coro::reset_cancellation_state_1_t<Filter> reset) noexcept
    {
        struct result
        {
            asio::cancellation_state & state;
            Filter filter_;
            token_type & token;

            bool await_ready() const noexcept
            {
                return true;
            }

            void await_suspend(std::coroutine_handle<void>) noexcept
            {
            }

            auto await_resume()
            {
                state = asio::cancellation_state(
                    asio::get_associated_cancellation_slot(token),
                    ASIO_MOVE_CAST(Filter)(filter_));
            }
        };

        return result{state, ASIO_MOVE_CAST(Filter)(reset.filter), token};
    }

    // This await transformation resets the associated cancellation state.
    template <typename InFilter, typename OutFilter>
    auto await_transform(
        asio::this_coro::reset_cancellation_state_2_t<InFilter, OutFilter> reset)
        noexcept
    {
        struct result
        {
            asio::cancellation_state & state;
            InFilter in_filter_;
            OutFilter out_filter_;
            token_type & token;


            bool await_ready() const noexcept
            {
                return true;
            }

            void await_suspend(std::coroutine_handle<void>) noexcept
            {
            }

            auto await_resume()
            {
                state = asio::cancellation_state(
                    asio::get_associated_cancellation_slot(token),
                    ASIO_MOVE_CAST(InFilter)(in_filter_),
                    ASIO_MOVE_CAST(OutFilter)(out_filter_));
            }
        };

        return result{state,
                      ASIO_MOVE_CAST(InFilter)(reset.in_filter),
                      ASIO_MOVE_CAST(OutFilter)(reset.out_filter),
                      token};
    }

    auto get_return_object() -> Return
    {
        return asio::async_initiate<Token, Sigs...>(
            [this](auto tk)
            {
                completion.emplace(std::move(tk));
            }, token);
    }

    void unhandled_exception()
    {
        throw ;
    }

    // TODO implement for overloads
    using completion_type = typename asio::async_completion<Token, Sigs...>::completion_handler_type;
    std::optional<completion_type> completion;
};

template<typename Return, typename Executor, typename Tag, typename Token, typename ... Args>
struct awaitable_compose_promise;

template<typename Return, typename Executor, typename ... Args_, typename Token, typename ... Args>
struct awaitable_compose_promise<Return, Executor, compose_tag<void(Args_...)>, Token, Args...>
    :  std::coroutine_traits<asio::awaitable<Return, Executor>>::promise_type
{
    using base_type = typename std::coroutine_traits<asio::awaitable<Return, Executor>>::promise_type;
    void return_value_impl(asio::error_code ec, Return && result)
    {
        if (ec)
            this->set_error(ec);
        else
            this->base_type::return_value(std::move(result));
    }
    void return_value_impl(std::exception_ptr e, Return && result)
    {
        if (e)
            this->set_except(e);
        else
            this->base_type::return_value(std::move(result));
    }

    void return_value_impl(Return && result)
    {
        this->base_type::return_value(std::move(result));
    }

    auto return_value(std::tuple<Args_ ...> result)
    {
        if constexpr (std::is_same_v<Return, std::tuple<Args_...>>)
            this->base_type::return_value(std::forward<Return>(result));
        else
            std::apply(
                [this](auto ... args)
                {
                    return_value_impl(std::move(args)...);
                }, std::move(result));
    }

    void unhandled_exception() { throw ; }
};



}

}

namespace std
{

// this is hack AF
template<typename Return, typename Executor, typename Tag, typename Token, typename ... Args>
struct coroutine_handle<asioex::detail::awaitable_compose_promise<Return, Executor, Tag, Token, Args...>>
    : coroutine_handle<typename std::coroutine_traits<asio::awaitable<Return, Executor>>::promise_type>
{
};

#define ASIOEX_TYPENAME(z, n, text) , typename T##n
#define ASIOEX_SPEC(z, n, text) , T##n
#define ASIOEX_TRAIT_DECL(z, n, text) \
template<typename Return BOOST_PP_REPEAT_2ND(n, ASIOEX_TYPENAME, ), typename Token, typename ... Sigs > \
struct coroutine_traits<Return BOOST_PP_REPEAT_2ND(n, ASIOEX_SPEC, ), Token, asioex::compose_tag<Sigs...>> \
{  \
    using promise_type = asioex::detail::compose_promise< \
                            Return, asioex::compose_tag<Sigs...>, Token         \
                            BOOST_PP_REPEAT_2ND(n, ASIOEX_SPEC, )>;     \
};

BOOST_PP_REPEAT(24, ASIOEX_TRAIT_DECL, );

#define ASIOEX_AW_TRAIT_DECL(z, n, text) \
template<typename Return, typename Executor BOOST_PP_REPEAT_2ND(n, ASIOEX_TYPENAME, ), typename Token, typename ... Sigs > \
struct coroutine_traits<asio::awaitable<Return, Executor> BOOST_PP_REPEAT_2ND(n, ASIOEX_SPEC, ), Token, asioex::compose_tag<Sigs...>> \
{  \
    using promise_type = asioex::detail::awaitable_compose_promise< \
                            Return, Executor, asioex::compose_tag<Sigs...>, Token \
                            BOOST_PP_REPEAT_2ND(n, ASIOEX_SPEC, )>;     \
};

BOOST_PP_REPEAT(24, ASIOEX_AW_TRAIT_DECL, );



#undef ASIOEX_TYPENAME
#undef ASIOEX_SPEC
#undef ASIOEX_TRAIT_DECL

}

#endif   // ASIO_EXPERIMENTS_ASYNC_HPP
