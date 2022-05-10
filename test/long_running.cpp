#include <asio/dispatch.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
#include <asioex/async_semaphore.hpp>

#include <iostream>
#include <mutex>
#include <thread>

using io_semaphore =
    asioex::basic_async_semaphore< asio::io_context::executor_type >;

std::mutex cout_mutex;

template < class... Ts >
void
println(Ts &&...xs)
{
    auto emit = [&](auto &&x) { std::cout << x; };

    auto lock = std::lock_guard< std::mutex >(cout_mutex);
    (emit(xs), ...);
    std::cout << '\n';
}

/// @brief A long-running task
/// @details This simulates for example, a synchronous database query
/// @param id is the identity of the long running task
/// @param sem is a reference to the semaphore to release once we have complete the task
void
long_running_task(std::string const &id, io_semaphore &sem)
{
    using namespace std::literals;

    println("long running task ", id, " starting");
    for (int i = 0; i < 4; ++i)
    {
        if (i != 0)
            std::this_thread::sleep_for(1s);
        println("long running task ", id, " tick ", i);
    }

    println("long running task ", id, " complete");
    // signal the waiter that we are done
    // Note: must dispatch to the correct executor as the async_semaphore is not
    // thread-safe
    asio::dispatch(sem.get_executor(), [&sem] { sem.release(); });
}

void
initiate(asio::io_context::executor_type  my_exec,
         asio::thread_pool::executor_type worker,
         std::string                      id)
{
    println("task ", id, " starting");

    // Create the semaphore with a release count of 0 so that initially any
    // acquire operation will suspend.
    auto sem  = std::make_unique< io_semaphore >(my_exec, 0);
    auto psem = sem.get();

    // post the long running task to the worker thread pool, passing a reference
    // to the semaphore so that the task can signal us when complete
    asio::post(worker, [id, psem] { long_running_task(id, *psem); });

    // asynchronously wait for the long-running task to complete.
    println("task ", id, " waiting");
    psem->async_acquire(
        [sem = std::move(sem), id](asio::error_code)
        {
            // this is the continuation code which is run once the long-running
            // task is complete
            println("task ", id, " complete");
        });
}

int
main()
{
    println("program starting");

    // This is our io context
    asio::io_context  ioc;

    // This is our thread pool
    asio::thread_pool workers;

    initiate(ioc.get_executor(), workers.get_executor(), "1");
    initiate(ioc.get_executor(), workers.get_executor(), "2");
    initiate(ioc.get_executor(), workers.get_executor(), "3");

    ioc.run();
    workers.join();

    println("program stopped");
}