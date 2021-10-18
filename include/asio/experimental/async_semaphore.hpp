#ifndef ASIO_EXPERIMENTAL_ASYNC_SEMAPHORE_HPP
#define ASIO_EXPERIMENTAL_ASYNC_SEMAPHORE_HPP

#include <asio/detail/config.hpp>
#include <asio/experimental/detail/bilist_node.hpp>

namespace asio
{
namespace experimental
{
struct semaphore_wait_op;

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

    inline bool
    try_acquire();

    inline void
    release();

  protected:
    inline void
    add_waiter(semaphore_wait_op *waiter);

    inline int
    decrement();

    ASIO_NODISCARD inline int
    count() const noexcept
    {
        return count_;
    }

  private:
    detail::bilist_node waiters_;
    int                 count_;
};

struct semaphore_wait_op : detail::bilist_node
{
    semaphore_wait_op(async_semaphore_base *host);

    virtual void complete(std::error_code) = 0;

    async_semaphore_base *host_;
};

}   // namespace experimental
}   // namespace asio

#include <asio/experimental/impl/semaphore_wait_op.hpp>
#include <asio/experimental/impl/async_semaphore_base.hpp>

#endif
