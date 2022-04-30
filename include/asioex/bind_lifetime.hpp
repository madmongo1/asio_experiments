// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_BIND_LIFETIME_HPP
#define ASIO_EXPERIMENTS_BIND_LIFETIME_HPP

#include <asio/async_result.hpp>
#include <asio/bind_allocator.hpp>

namespace asioex
{

template<typename T, typename Lifetime>
struct lifetime_binder
{
    template<typename Lifetime_, typename T_>
    lifetime_binder(Lifetime_ && lifetime, T_ && t) :
            lifetime_(std::forward<Lifetime_>(lifetime)),
            target_(std::forward<T_>(t)) {}

    using lifetime_type = Lifetime;
    using target_type = T;


    /// Obtain a reference to the target object.
    target_type& get() ASIO_NOEXCEPT
    {
        return target_;
    }

    /// Obtain a reference to the target object.
    const target_type& get() const ASIO_NOEXCEPT
    {
        return target_;
    }

    /// Obtain the associated lifetime.
    lifetime_type get_lifetime() const ASIO_NOEXCEPT
    {
        return lifetime_;
    }

    /// Forwarding function call operator.
    template <typename... Args>
    typename asio::result_of<T(Args...)>::type operator()(
        ASIO_MOVE_ARG(Args)... args)
    {
        return target_(ASIO_MOVE_CAST(Args)(args)...);
    }

    /// Forwarding function call operator.
    template <typename... Args>
    typename asio::result_of<T(Args...)>::type operator()(
        ASIO_MOVE_ARG(Args)... args) const
    {
        return target_(ASIO_MOVE_CAST(Args)(args)...);
    }


  private:
    Lifetime lifetime_;
    T target_;

};


/// Associate an object of type @c T with an allocator of type
/// @c Lifetime.
template <typename Lifetime , typename T>
ASIO_NODISCARD inline lifetime_binder<typename std::decay<T>::type, Lifetime>
bind_lifetime(const Lifetime & s, ASIO_MOVE_ARG(T) t)
{
    return lifetime_binder<
        typename std::decay<T>::type, Lifetime>(
        s, ASIO_MOVE_CAST(T)(t));
}

}

namespace asio
{


template <typename T, typename Lifetime, typename Signature>
struct async_result<asioex::lifetime_binder<T, Lifetime>, Signature> :
    public asio::detail::allocator_binder_async_result_completion_handler_type<
        async_result<T, Signature>, Lifetime>,
    public asio::detail::allocator_binder_async_result_return_type<
        async_result<T, Signature> >
{

    explicit async_result(asioex::lifetime_binder<T, Lifetime>& b)
    : target_(b.get())
    {
    }

    typename async_result<T, Signature>::return_type get()
    {
        return target_.get();
    }

    template <typename Initiation>
    struct init_wrapper
    {
        template <typename Init>
        init_wrapper(const Lifetime& allocator, ASIO_MOVE_ARG(Init) init)
        : lifetime_(allocator),
        initiation_(ASIO_MOVE_CAST(Init)(init))
        {
        }

        template <typename Handler, typename... Args>
        void operator()(
            ASIO_MOVE_ARG(Handler) handler,
            ASIO_MOVE_ARG(Args)... args)
        {
            ASIO_MOVE_CAST(Initiation)(initiation_)(
                asioex::lifetime_binder<
                    typename decay<Handler>::type, Lifetime>(
                    lifetime_, ASIO_MOVE_CAST(Handler)(handler)),
                ASIO_MOVE_CAST(Args)(args)...);
        }

        template <typename Handler, typename... Args>
        void operator()(
            ASIO_MOVE_ARG(Handler) handler,
            ASIO_MOVE_ARG(Args)... args) const
        {
            initiation_(
                asioex::lifetime_binder<
                    typename decay<Handler>::type, Lifetime>(
                    lifetime_, ASIO_MOVE_CAST(Handler)(handler)),
                ASIO_MOVE_CAST(Args)(args)...);
        }

        Lifetime lifetime_;
        Initiation initiation_;
    };

    template <typename Initiation, typename RawCompletionToken, typename... Args>
    static ASIO_INITFN_DEDUCED_RESULT_TYPE(T, Signature,
                                           (async_initiate<T, Signature>(
                                            declval<init_wrapper<typename decay<Initiation>::type> >(),
                                            declval<RawCompletionToken>().get(),
                                            declval<ASIO_MOVE_ARG(Args)>()...)))
        initiate(
            ASIO_MOVE_ARG(Initiation) initiation,
            ASIO_MOVE_ARG(RawCompletionToken) token,
            ASIO_MOVE_ARG(Args)... args)
    {
        return async_initiate<T, Signature>(
            init_wrapper<typename decay<Initiation>::type>(
                token.get_lifetime(),
                ASIO_MOVE_CAST(Initiation)(initiation)),
            token.get(), ASIO_MOVE_CAST(Args)(args)...);
    }
    
    async_result<T, Signature> target_;

};

}

#endif   // ASIO_EXPERIMENTS_BIND_LIFETIME_HPP
