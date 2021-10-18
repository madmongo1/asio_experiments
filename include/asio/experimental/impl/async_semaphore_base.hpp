namespace asio
{
namespace experimental
{
async_semaphore_base::async_semaphore_base(int initial_count)
: waiters_()
, count_(initial_count)
{
}

async_semaphore_base::~async_semaphore_base()
{
    detail::bilist_node *p = &waiters_;
    while (p->next_ != &waiters_)
    {
        detail::bilist_node *current = p;
        p                            = p->next_;
        static_cast< semaphore_wait_op * >(current)->complete(
            error::operation_aborted);
    }
}

void
async_semaphore_base::add_waiter(semaphore_wait_op *waiter)
{
    waiter->link_before(&waiters_);
}

void
async_semaphore_base::release()
{
    count_ += 1;

    // release a pending operations
    if (waiters_.next_ == &waiters_)
        return;

    decrement();
    static_cast< semaphore_wait_op * >(waiters_.next_)
        ->complete(std::error_code());
}

bool
async_semaphore_base::try_acquire()
{
    bool acquired = false;
    if (count_ > 0)
    {
        --count_;
        acquired = true;
    }
    return acquired;
}

int
async_semaphore_base::decrement()
{
    assert(count_ > 0);
    return --count_;
}

}   // namespace experimental
}   // namespace asio