// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_REDIRECT_CANCELLATION_HPP
#define ASIO_EXPERIMENTS_REDIRECT_CANCELLATION_HPP


#include <asio/async_result.hpp>
#include <asio/associator.hpp>
#include <asio/error.hpp>
#include <asio/experimental/channel_error.hpp>
#include <asio/associated_cancellation_slot.hpp>
#include <asio/cancellation_type.hpp>
#include <asio/cancellation_signal.hpp>
#include <asio/cancellation_state.hpp>

namespace asioex
{

template <typename CompletionToken>
class redirect_cancellation_t
{
  public:
    /// Constructor.
    template <typename T>
    redirect_cancellation_t(ASIO_MOVE_ARG(T) completion_token,
                            asio::cancellation_type& cancel)
        : token_(ASIO_MOVE_CAST(T)(completion_token)),
          cancel_(cancel)
    {
    }

    //private:
    CompletionToken token_;
    asio::cancellation_type& cancel_;
    asio::cancellation_state state_;
};


/// Adapt a @ref completion_token to capture error_code values to a variable.
template <typename CompletionToken>
inline redirect_cancellation_t<typename std::decay<CompletionToken>::type> redirect_cancellation(
    ASIO_MOVE_ARG(CompletionToken) completion_token,
    asio::cancellation_type& cancel)
{
    return redirect_cancellation_t<typename std::decay<CompletionToken>::type>(
        ASIO_MOVE_CAST(CompletionToken)(completion_token), cancel);
}

namespace detail {

// Class to adapt a redirect_cancellation_t as a completion handler.
template <typename Handler>
class redirect_cancellation_handler
{
  public:
    typedef void result_type;

    template <typename CompletionToken>
    redirect_cancellation_handler(redirect_cancellation_t<CompletionToken> e)
    : cancel_(e.cancel_),
    handler_(ASIO_MOVE_CAST(CompletionToken)(e.token_))
    {
    }

    template <typename RedirectedHandler>
    redirect_cancellation_handler(asio::cancellation_type& cancel,
                                  ASIO_MOVE_ARG(RedirectedHandler) h)
    : cancel_(cancel),
    handler_(ASIO_MOVE_CAST(RedirectedHandler)(h))
    {
    }

    void operator()()
    {
        ASIO_MOVE_OR_LVALUE(Handler)(handler_)();
    }

    template <typename Arg, typename... Args>
    typename std::enable_if<
        !std::is_same<typename std::decay<Arg>::type, asio::error_code>::value
        >::type
    operator()(ASIO_MOVE_ARG(Arg) arg, ASIO_MOVE_ARG(Args)... args)
    {
        ASIO_MOVE_OR_LVALUE(Handler)(handler_)(
            ASIO_MOVE_CAST(Arg)(arg),
            ASIO_MOVE_CAST(Args)(args)...);
    }

    template <typename... Args>
    void operator()(asio::error_code ec,
               ASIO_MOVE_ARG(Args)... args)
    {
        if ((cancel_ = state_.cancelled()) != asio::cancellation_type::none)
            ec = asio::error_code{};
        ASIO_MOVE_OR_LVALUE(Handler)(handler_)(ec, ASIO_MOVE_CAST(Args)(args)...);
    }

    using cancellation_slot_type = asio::cancellation_slot;
    cancellation_slot_type get_cancellation_slot() const noexcept
    {
        return state_.slot();
    }

    static asio::cancellation_type no_filter(asio::cancellation_type c)
    {
        return c;
    }

    //private:
    asio::cancellation_type& cancel_;
    Handler handler_;
    asio::cancellation_state state_{ asio::get_associated_cancellation_slot(handler_), &no_filter};
};


} // namespace detail


}

namespace asio {

#if !defined(GENERATING_DOCUMENTATION)

template <typename CompletionToken, typename Signature>
struct async_result<asioex::redirect_cancellation_t<CompletionToken>, Signature>
{
    typedef typename async_result<CompletionToken, Signature>
        ::return_type return_type;

    template <typename Initiation>
    struct init_wrapper
    {
        template <typename Init>
        init_wrapper(asio::cancellation_type& cancel, ASIO_MOVE_ARG(Init) init)
        : cancel_(cancel),
        initiation_(ASIO_MOVE_CAST(Init)(init))
        {
        }


        template <typename Handler, typename... Args>
        void operator()(
            ASIO_MOVE_ARG(Handler) handler,
            ASIO_MOVE_ARG(Args)... args)
        {
            ASIO_MOVE_CAST(Initiation)(initiation_)(
                asioex::detail::redirect_cancellation_handler<
                    typename decay<Handler>::type>(
                    cancel_, ASIO_MOVE_CAST(Handler)(handler)),
                ASIO_MOVE_CAST(Args)(args)...);
        }


        asio::cancellation_type& cancel_;
        Initiation initiation_;
    };

    template <typename Initiation, typename RawCompletionToken, typename... Args>
    static return_type initiate(
        ASIO_MOVE_ARG(Initiation) initiation,
        ASIO_MOVE_ARG(RawCompletionToken) token,
        ASIO_MOVE_ARG(Args)... args)
    {

        return async_initiate<CompletionToken, Signature>(
            init_wrapper<typename decay<Initiation>::type>(
                token.cancel_, ASIO_MOVE_CAST(Initiation)(initiation)),
            token.token_, ASIO_MOVE_CAST(Args)(args)...);
    }

};

template <template <typename, typename> class Associator,
           typename Handler, typename DefaultCandidate>
struct associator<Associator,
                   asioex::detail::redirect_cancellation_handler<Handler>, DefaultCandidate>
: Associator<Handler, DefaultCandidate>
{
    static typename Associator<Handler, DefaultCandidate>::type get(
        const asioex::detail::redirect_cancellation_handler<Handler>& h,
        const DefaultCandidate& c = DefaultCandidate()) ASIO_NOEXCEPT
    {
        return Associator<Handler, DefaultCandidate>::get(h.handler_, c);
    }
};

#endif // !defined(GENERATING_DOCUMENTATION)

} // namespace asio

#endif   // ASIO_EXPERIMENTS_REDIRECT_CANCELLATION_HPP
