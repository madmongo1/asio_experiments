// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_ASYNC_OP_HPP
#define ASIO_EXPERIMENTS_ASYNC_OP_HPP

#include <asio/any_io_executor.hpp>
#include <asio/associated_allocator.hpp>
#include <asio/associated_cancellation_slot.hpp>
#include <asio/associated_executor.hpp>
#include <functional>

namespace asioex
{

namespace detail
{

template<typename Signature,
         typename Executor = asio::any_io_executor,
         typename Allocator = std::allocator<void>,
         typename CancellationSlot = asio::cancellation_slot>
struct poly_handler_base;

template<typename ... Args,
           typename Executor,
           typename Allocator,
           typename CancellationSlot>
struct poly_handler_base<void(Args...), Executor, Allocator, CancellationSlot>
{

    using executor_type = Executor;
    executor_type get_executor() const
    {
        return executor;
    }

    using allocator_type = Allocator;
    allocator_type get_allocator() const
    {
        return allocator;
    }

    using cancellation_slot_type = asio::cancellation_slot;
    cancellation_slot_type get_cancellation_slot() const
    {
        return cancellation_slot;
    }
    executor_type executor;
    allocator_type allocator;
    cancellation_slot_type cancellation_slot;

    poly_handler_base(Executor executor,
         Allocator allocator,
         CancellationSlot cancellation_slot
         ) : executor(std::move(executor)), allocator(std::move(allocator)), cancellation_slot(std::move(cancellation_slot))
    {
    }

    virtual void operator()(Args  && ... args) && = 0;

    virtual ~poly_handler_base() = default;
};

template<typename CompletionHandler,
         typename Signature,
         typename Executor = asio::any_io_executor,
         typename Allocator = std::allocator<void>,
         typename CancellationSlot = asio::cancellation_slot>
struct poly_handler_impl;


template<typename CompletionHandler,
         typename ... Args,
         typename Executor,
         typename Allocator,
         typename CancellationSlot>
struct poly_handler_impl<CompletionHandler, void(Args...), Executor, Allocator, CancellationSlot> final
    : poly_handler_base<void(Args...), Executor, Allocator, CancellationSlot>
{

    poly_handler_impl(Executor executor,
             Allocator allocator,
             CancellationSlot cancellation_slot,
             CompletionHandler completion_handler) :
        poly_handler_base<void(Args...), Executor, Allocator, CancellationSlot>(
            std::move(executor), std::move(allocator), std::move(cancellation_slot)),
            handler(std::move(completion_handler))
    {
    }

    CompletionHandler handler;

    void operator()(Args  && ... args) && override
    {
        std::move(handler)(std::forward<Args>(args)...);
    }
};

template<typename Signature,
           typename Executor = asio::any_io_executor,
           typename Allocator = std::allocator<void>,
           typename CancellationSlot = asio::cancellation_slot>
struct poly_handler;


template<typename ... Args,
           typename Executor,
           typename Allocator,
           typename CancellationSlot>
struct poly_handler<void(Args...), Executor, Allocator, CancellationSlot> final
{
    using executor_type = Executor;
    executor_type get_executor() const
    {
        assert(ptr_);
        return ptr_->get_executor();
    }

    using allocator_type = Allocator;
    allocator_type get_allocator() const
    {
        assert(ptr_);
        return ptr_->get_allocator();
    }

    using cancellation_slot_type = asio::cancellation_slot;
    cancellation_slot_type get_cancellation_slot() const
    {
        assert(ptr_);
        return ptr_->get_cancellation_slot();
    }

    template<typename CompletionToken>
    poly_handler(CompletionToken && token,
                 Executor executor,
                 Allocator allocator,
                 CancellationSlot cancellation_slot)
        : ptr_(std::make_unique<
            poly_handler_impl<std::decay_t<CompletionToken>, void(Args...), Executor, Allocator, CancellationSlot>>(
                                   std::move(executor), std::move(allocator),
                                   std::move(cancellation_slot), std::forward<CompletionToken>(token)))
    {

    }

    void operator()(Args  ... args)
    {
        assert(ptr_);
        std::move(*std::exchange(ptr_, nullptr))(std::move(args)...);
    };

  private:
    std::unique_ptr<poly_handler_base<void(Args...), Executor, Allocator, CancellationSlot>> ptr_;
};



}



template<typename Signature,
         typename Executor = asio::any_io_executor,
         typename Allocator = std::allocator<void>,
         typename CancellationSlot = asio::cancellation_slot>
struct basic_async_op;

template<typename ... Args,
           typename Executor,
           typename Allocator,
           typename CancellationSlot>
struct basic_async_op<void(Args...), Executor, Allocator, CancellationSlot>
{

    using executor_type = Executor;
    executor_type get_executor() const
    {
        assert(impl_);
        return impl_->executor;
    }

    using allocator_type = Allocator;
    allocator_type get_allocator() const
    {
        assert(impl_);
        return impl_->allocator;
    }

    using cancellation_slot_type = asio::cancellation_slot;
    cancellation_slot_type get_cancellation_slot() const
    {
        assert(impl_);
        return impl_->cancellation_slot;
    }

    template <
        ASIO_COMPLETION_TOKEN_FOR(void (Args...))
            CompletionToken ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken,
                                 void (Args...))
    operator()(ASIO_MOVE_ARG(CompletionToken) token ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return asio::async_initiate<CompletionToken, void(Args...)>(initiate{impl_.get()}, token);
    }


    struct base
    {
        Executor executor;
        Allocator allocator;
        CancellationSlot cancellation_slot;
        base(Executor executor,
            Allocator allocator,
            CancellationSlot cancellation_slot
            ) : executor(std::move(executor)), allocator(std::move(allocator)), cancellation_slot(std::move(cancellation_slot))
        {
        }

        virtual ~base() = default;
        virtual void operator()(detail::poly_handler<void(Args...), Executor, Allocator, CancellationSlot>) = 0;
    };

    template<typename Initiation, typename ... Ts>
    struct impl final : base
    {
        Initiation initiation;
        std::tuple<Ts...> args;

        impl(Executor executor,
             Allocator allocator,
             CancellationSlot cancellation_slot,
             Initiation init, Ts && ... args) :
            base(std::move(executor), std::move(allocator), std::move(cancellation_slot)),
            initiation(std::move(init)), args(std::forward<Ts>(args)...) {}

        template<std::size_t ... Idx>
        void invoke(detail::poly_handler<void(Args...), Executor, Allocator, CancellationSlot> ph,
                    std::index_sequence<Idx...>)
        {
            std::move(initiation)(std::move(ph), std::get<Idx>(args)...);
        }

        void operator()(detail::poly_handler<void(Args...), Executor, Allocator, CancellationSlot> ph) override
        {
            this->template invoke(std::move(ph), std::make_index_sequence<sizeof...(Ts)>{});
        }

    };

    basic_async_op(std::unique_ptr<base> impl) : impl_(std::move(impl)) {}

    struct initiate
    {
        base * ptr;

        template <typename Handler>
        void operator()(Handler handler)
        {
            detail::poly_handler<void(Args...), Executor, Allocator, CancellationSlot> h{
                std::move(handler),
                asio::get_associated_executor(handler, ptr->executor),
                asio::get_associated_allocator(handler, ptr->allocator),
                asio::get_associated_cancellation_slot(handler, ptr->cancellation_slot)};

                (*ptr)(std::move(h));
        }
    };

  private:
    std::unique_ptr<base> impl_;
};

template<typename Executor = asio::any_io_executor,
         typename Allocator = std::allocator<void>,
         typename CancellationSlot = asio::cancellation_slot>
struct as_async_op_t
{

    /// Adapts an executor to add the @c as_async_op_t completion token as the
    /// default.
    template <typename InnerExecutor>
    struct executor_with_default : InnerExecutor
    {
        /// Specify @c as_async_op_t as the default completion token type.
        typedef as_async_op_t default_completion_token_type;
        
        /// Construct the adapted executor from the inner executor type.
        template <typename InnerExecutor1>
        executor_with_default(const InnerExecutor1& ex,
                              typename asio::constraint<
                                  asio::conditional<
                                      !std::is_same<InnerExecutor1, executor_with_default>::value,
                                      std::is_convertible<InnerExecutor1, InnerExecutor>,
                                      std::false_type
                                      >::type::value
                                  >::type = 0) ASIO_NOEXCEPT
        : InnerExecutor(ex)
        {
        }
    };

    
    /// Type alias to adapt an I/O object to use @c as_async_op_t as its
    /// default completion token type.
#if defined(ASIO_HAS_ALIAS_TEMPLATES) \
  || defined(GENERATING_DOCUMENTATION)
    template <typename T>
    using as_default_on_t = typename T::template rebind_executor<
        executor_with_default<typename T::executor_type> >::other;
#endif // defined(ASIO_HAS_ALIAS_TEMPLATES)
         //   || defined(GENERATING_DOCUMENTATION)
         
    /// Function helper to adapt an I/O object to use @c as_async_op_t as its
    /// default completion token type.
    template <typename T>
    static typename std::decay<T>::type::template rebind_executor<
        executor_with_default<typename std::decay<T>::type::executor_type>
        >::other
    as_default_on(ASIO_MOVE_ARG(T) object)
    {
        return typename std::decay<T>::type::template rebind_executor<
            executor_with_default<typename std::decay<T>::type::executor_type>
            >::other(ASIO_MOVE_CAST(T)(object));
    }

};

constexpr as_async_op_t as_async_op;

template<typename Signature>
using async_op = basic_async_op<Signature>;

}

namespace asio
{

template<typename ... Props, typename ... Args>
struct async_result<asioex::as_async_op_t<Props...>, void(Args...)>
{
    using result_type = asioex::basic_async_op<void(Args...), Props...>;
    template <typename Initiation, typename... InitArgs>
    static result_type initiate(ASIO_MOVE_ARG(Initiation) initiation,
             asioex::as_async_op_t<Props...> tk,
             ASIO_MOVE_ARG(InitArgs)... init_args)
    {
        using poly = asioex::detail::poly_handler<void(Args...), Props...>;
        using base = typename result_type::base;
        using impl = typename result_type::template impl<std::decay_t<Initiation>, std::decay_t<InitArgs>...>;

        std::unique_ptr<base> pp = std::make_unique<impl>(
            asio::get_associated_executor(initiation),
            asio::get_associated_allocator(initiation),
            asio::get_associated_cancellation_slot(initiation),
            std::move(initiation),
            std::forward<InitArgs>(init_args)...);
        return {std::move(pp)};
    }
};

}

#endif   // ASIO_EXPERIMENTS_ASYNC_OP_HPP
