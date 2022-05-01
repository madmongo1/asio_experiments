#ifndef ASIOEX_ASYNC_SEMAPHORE_HPP
#define ASIOEX_ASYNC_SEMAPHORE_HPP

#include <asio/any_io_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/detail/config.hpp>
#include <asio/experimental/prepend.hpp>
#include <asioex/detail/bilist_node.hpp>
#include <asioex/error_code.hpp>

namespace asioex
{
namespace detail
{
struct semaphore_wait_op;
}

struct async_semaphore_base
{
    inline async_semaphore_base(int initial_count);

    async_semaphore_base(async_semaphore_base const &) ASIO_DELETED;

    async_semaphore_base &
    operator=(async_semaphore_base const &) ASIO_DELETED;

    async_semaphore_base(async_semaphore_base &&) ASIO_DELETED;

    async_semaphore_base &
    operator=(async_semaphore_base &&) ASIO_DELETED;

    inline ~async_semaphore_base();

    /// @brief Attempt to immediately acquire the semaphore.
    /// @details This function attempts to acquire the semaphore without
    /// blocking or initiating an asynchronous operation.
    /// @returns true if the semaphore was acquired, false otherwise
    inline bool
    try_acquire();

    /// @brief Release the sempahore.
    /// @details This function immediately releases the semaphore. If there are
    /// pending async_acquire operations, then the least recent operation will
    /// commence completion.
    inline void
    release();

    /// @brief Release the sempahore to achieve a value of zero.
    /// @returns The amount of releases.
    /// @details This function immediately releases the semaphore. If there are
    /// pending async_acquire operations, then the least recent operation will
    /// commence completion.
    inline std::size_t
    release_all();

    ASIO_NODISCARD inline int
    value() const noexcept;

  protected:
    inline void
    add_waiter(detail::semaphore_wait_op *waiter);

    inline int
    decrement();

    ASIO_NODISCARD inline int
    count() const noexcept;

  private:
    detail::bilist_node waiters_;
    int                 count_;
};

template < class Executor = asio::any_io_executor >
struct basic_async_semaphore : async_semaphore_base
{
    /// @brief The type of the default executor.
    using executor_type = Executor;

    /// Rebinds the socket type to another executor.
    template < typename Executor1 >
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef basic_async_semaphore< Executor1 > other;
    };

    /// @brief Construct an async_sempaphore
    /// @param exec is the default executor associated with the async_semaphore
    /// @param initial_count is the initial value of the internal counter.
    /// @pre initial_count >= 0
    /// @pre initial_count <= MAX_INT
    ///
    basic_async_semaphore(executor_type exec, int initial_count = 1);

    /// @brief return the default executor.
    executor_type const &
    get_executor() const;

    /// @brief Initiate an asynchronous acquire of the semaphore
    /// @details Multiple asynchronous acquire operations may be in progress at
    /// the same time. However, the caller must ensure that this function is not
    /// invoked from two threads simultaneously. When the semaphore's internal
    /// count is above zero, async acquire operations will complete in strict
    /// FIFO order. If the semaphore object is destoyed while an async_acquire
    /// is outstanding, the operation's completion handler will be invoked with
    /// the error_code set to error::operation_aborted. If the async_acquire
    /// operation is cancelled before completion, the completion handler will be
    /// invoked with the error_code set to error::operation_aborted. Successful
    /// acquisition of the semaphore is signalled to the caller when the
    /// completion handler is invoked with no error.
    /// @tparam CompletionHandler represents a completion token or handler which
    /// is invokable with the signature `void(error_code)`
    /// @param token is a completion token or handler matching the signature
    /// void(error_code)
    /// @note The completion handler will be invoked as if by `post` to the
    /// handler's associated executor. If no executor is associated with the
    /// completion handler, the handler will be invoked as if by `post` to the
    /// async_semaphore's associated default executor.
    template < ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionHandler
                   ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type) >
    ASIO_INITFN_RESULT_TYPE(CompletionHandler, void(error_code))
    async_acquire(
        CompletionHandler &&token ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));

  private:
    executor_type exec_;
};

using async_semaphore = basic_async_semaphore<>;

template<typename Executor, typename Op, typename Signature>
struct synchronized_op;

template<typename Executor, typename Op, typename Err, typename ... Args>
struct synchronized_op<Executor, Op, void (Err, Args...)>
{
    basic_async_semaphore<Executor> & sm;
    Op op;

    struct semaphore_tag {};
    struct op_tag {};

    static auto make_error_impl(error_code ec, error_code *)
    {
        return ec;
    }

    static auto make_error_impl(error_code ec, std::exception_ptr *)
    {
        return std::make_exception_ptr(std::system_error(ec));
    }

    static auto make_error(error_code ec)
    {
        return make_error_impl(ec, static_cast<Err*>(nullptr));
    }

    template<typename Self>
    void operator()(Self && self) // init
    {
        if (self.get_cancellation_state().cancelled() != asio::cancellation_type::none)
            return std::move(self).complete(make_error(asio::error::operation_aborted), Args{}...);

        sm.async_acquire(
            asio::experimental::prepend(std::move(self), semaphore_tag{}));
    }

    template<typename Self>
    void operator()(Self && self, semaphore_tag, error_code ec) // semaphore obtained
    {
        std::move(op)(asio::experimental::prepend(std::move(self), op_tag{}));
    }

    template<typename Self, typename ... Args_>
    void operator()(Self && self, op_tag, Args_ &&  ... args ) // semaphore obtained
    {
        sm.release();
        std::move(self).complete(std::forward<Args_>(args)...);
    }
};



/// Function to run OPs only when the semaphore can be acquired.
/// That way an artificial number of processes can run in parallel.
template<typename Executor, typename Op,
         ASIO_COMPLETION_TOKEN_FOR(typename decltype(std::declval<Op>()(asio::experimental::detail::deferred_signature_probe{}))::type)
               CompletionToken
               ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
auto synchronized(basic_async_semaphore<Executor> & sm,
                  Op && op,
                CompletionToken && completion_token)
{
    using sig_t = typename decltype(std::declval<Op>()(asio::experimental::detail::deferred_signature_probe{}))::type;

    using cop = synchronized_op<Executor, std::decay_t<Op>, sig_t>;

    return asio::async_compose<CompletionToken, sig_t>(cop{sm, std::forward<Op>(op)}, completion_token, sm);

}

}   // namespace asioex

#endif

#include <asioex/impl/async_semaphore_base.hpp>
#include <asioex/impl/basic_async_semaphore.hpp>
