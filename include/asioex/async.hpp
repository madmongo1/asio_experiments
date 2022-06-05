// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_ASYNC_HPP
#define ASIO_EXPERIMENTS_ASYNC_HPP

#include <asio/compose.hpp>
#include <asio/post.hpp>
#include <asio/dispatch.hpp>

#include <asio/experimental/deferred.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/this_coro.hpp>

#include <coroutine>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <boost/preprocessor/repeat.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/preprocessor/repeat_2nd.hpp>
#include <boost/smart_ptr/allocate_unique.hpp>

#include <optional>
#include <variant>


namespace asioex
{

using          async_token_t = asio::experimental::deferred_t;
constexpr auto async_token   = asio::experimental::deferred;


template<typename ... Signatures>
struct async
{
    // helper function used in the macro
    template<typename Token, typename Initiation, typename ... Args>
    static auto initiate(Initiation && initiation, Token && token, Args && ... args)
    {
        return asio::async_initiate<Token, Signatures...>(
             std::forward<Initiation>(initiation),
             std::forward<Token>(token),
             std::forward<Args>(args)...);
    }

};


namespace detail
{


template<typename CompletionHandler>
auto pick_executor(CompletionHandler && completion_handler)
{
    return asio::get_associated_executor(completion_handler);
}


template<typename CompletionHandler,
         typename First,
         typename ... IoObjectsOrExecutors>
auto pick_executor(CompletionHandler && completion_handler,
                   const First & first,
                   IoObjectsOrExecutors && ... io_objects_or_executors)
    -> typename std::enable_if<
        asio::is_executor<First>::value || asio::execution::is_executor<First>::value
        ,First >::type
{
    return first;
}


template<typename CompletionHandler,
           typename First,
           typename ... IoObjectsOrExecutors>
auto pick_executor(CompletionHandler && completion_handler,
                   First & first,
                   IoObjectsOrExecutors && ... io_objects_or_executors)
    -> typename First::executor_type
{
    return first.get_executor();
}



template<typename CompletionHandler,
           typename First,
           typename ... IoObjectsOrExecutors>
auto pick_executor(CompletionHandler && completion_handler,
                   First &&,
                   IoObjectsOrExecutors && ... io_objects_or_executors)
{
    return pick_executor(std::forward<CompletionHandler>(completion_handler),
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

template<typename Return, typename CompletionHandler, typename ... Args>
struct compose_promise;

template<typename Allocator, typename CompletionHandler, typename ... Args>
struct compose_promise_alloc_base
{
    using allocator_type = Allocator;
    void* operator new(const std::size_t size,
                 Args & ... args, CompletionHandler & tk)
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

template<typename Tag, typename CompletionHandler, typename ... Args>
struct compose_promise_alloc_base<std::allocator<void>, Tag, CompletionHandler, Args...>
{
};


template<typename ...Sigs, typename CompletionHandler, typename ... Args>
struct compose_promise<async<Sigs...>, CompletionHandler, Args...>
    :
    compose_promise_alloc_base<
        asio::associated_allocator_t<std::decay_t<CompletionHandler>>, CompletionHandler, Args...>,
    compose_promise_base<compose_promise<async<Sigs...>, CompletionHandler, Args...>, Sigs> ...
{
    using my_type = compose_promise<async<Sigs...>, CompletionHandler, Args...>;
    using compose_promise_base<my_type, Sigs> ::return_value ...;
    using result_type = std::variant<typename compose_promise_base<my_type, Sigs>::tuple_type ...>;

    using completion_handler_type = std::decay_t<CompletionHandler>;

    std::optional<result_type> result_;

    completion_handler_type completion_handler;
    using allocator_type = asio::associated_allocator_t<completion_handler_type>;

    asio::cancellation_state state{
        asio::get_associated_cancellation_slot(completion_handler),
        asio::enable_terminal_cancellation()
    };

    using executor_type =
        typename asio::prefer_result<
            decltype(pick_executor(std::declval<CompletionHandler>(), std::declval<Args>()...)),
            asio::execution::outstanding_work_t::tracked_t>::type;

    executor_type executor_;
    bool did_suspend = false;

#if defined(__clang__) || defined(_MSC_FULL_VER)
    compose_promise(Args &... args, CompletionHandler & tk)
        : completion_handler(static_cast<CompletionHandler>(tk)), executor_(
          asio::prefer(
            pick_executor(completion_handler, args...),
              asio::execution::outstanding_work.tracked))
    {
    }
#else
    compose_promise(Args &... args, CompletionHandler && tk)
    : completion_handler(static_cast<CompletionHandler>(tk)), executor_(
          asio::prefer(
              pick_executor(completion_handler, args...),
              asio::execution::outstanding_work.tracked))
    {
    }
#endif

    ~compose_promise()
    {
        if (result_)
            std::visit(
                [this](auto & tup)
                {
                    auto cpl =
                            [tup = std::move(tup),
                             completion = std::move(completion_handler)]() mutable
                            {
                                std::apply(std::move(completion), std::move(tup));
                            };
                    if (did_suspend)
                        asio::dispatch(executor_, std::move(cpl));
                    else
                        asio::post(executor_, std::move(cpl));
                }, *result_);
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
                    return asio::get_associated_allocator(self->completion_handler);
                }

                void operator()(Args_ ... args)
                {
                    self->did_suspend  = true;
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
            completion_handler_type & completion_handler;

            bool await_ready() const noexcept
            {
                return true;
            }

            void await_suspend(std::coroutine_handle<void>) noexcept
            {
            }

            auto await_resume() const
            {
                state = asio::cancellation_state(asio::get_associated_cancellation_slot(completion_handler));
            }
        };
        return result{state, completion_handler};
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
            completion_handler_type & completion_handler;

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
                    asio::get_associated_cancellation_slot(completion_handler),
                    ASIO_MOVE_CAST(Filter)(filter_));
            }
        };

        return result{state, ASIO_MOVE_CAST(Filter)(reset.filter), completion_handler};
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
            completion_handler_type & completion_handler;


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
                    asio::get_associated_cancellation_slot(completion_handler),
                    ASIO_MOVE_CAST(InFilter)(in_filter_),
                    ASIO_MOVE_CAST(OutFilter)(out_filter_));
            }
        };

        return result{state,
                      ASIO_MOVE_CAST(InFilter)(reset.in_filter),
                      ASIO_MOVE_CAST(OutFilter)(reset.out_filter),
                      completion_handler};
    }

    auto get_return_object() -> async<Sigs...>
    {
        return  {};
    }

    template<typename ... Args_, typename ... Rest>
    void complete(async<void(Args_...), Rest...> )
    {

    }

    void unhandled_exception()
    {
        // mangle it onto the executor so the coro dies safely
        asio::post(executor_,
                  [ex = std::current_exception()]
                  {
                      std::rethrow_exception(ex);
                  });

    }
};

}

}

namespace std
{

template<typename ...Sigs, typename ... Args>
struct coroutine_traits<asioex::async<Sigs...>, Args...>
{
    using tuple_type = std::tuple<Args...>;
    using handler_type = std::tuple_element_t<sizeof...(Args) -1, tuple_type>;
    using idx_seq = std::make_index_sequence<sizeof...(Args) - 1>;

    template<std::size_t ... Idx>
    constexpr static  auto make_promise_type_impl(std::index_sequence<Idx...>)
        //-> std::tuple< std::tuple_element_t<Idx, tuple_type>...>;
        ->  asioex::detail::compose_promise<
                asioex::async<Sigs...>,
                handler_type,
                std::tuple_element_t<Idx, tuple_type>...>;
    using promise_type = decltype(make_promise_type_impl(idx_seq{}));
};


}

#endif   // ASIO_EXPERIMENTS_ASYNC_HPP
