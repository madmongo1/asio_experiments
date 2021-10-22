#include <asio.hpp>

#include <asioex/error_code.hpp>
#include <asioex/tick_state.hpp>

#include <assert.h>

struct copy_report
{
    asio::error_code read_error, write_error;
    std::size_t      read_bytes = 0, write_bytes = 0;
};

template < class ConstBufferSequence >
std::array< asio::const_buffer, 8 >
subsequence(ConstBufferSequence seq, std::size_t start, std::size_t len)
{
    std::array< asio::const_buffer, 8 > result;

    auto first_d = asio::buffer_sequence_begin(result);
    auto last_d  = asio::buffer_sequence_end(result);

    std::size_t copied = 0;

    auto first_s = asio::buffer_sequence_begin(seq);
    auto last_s  = asio::buffer_sequence_end(seq);

    asio::const_buffer current;

    // find the const_buffer to start
    while(first_s != last_s && current.size() == 0)
    {
        current = *first_s++;
        auto adjust = (std::min)(current.size(), start);
        current += adjust;
        start -= adjust;
    }

    // current now contains the first buffer, adjusted to be at the start
    // of the subsequence

    // check to see whether the first is the last
    if (current.size() > len)
    {
        current    = asio::const_buffer(current.data(), len);
        *first_d++ = current;
        return result;
    }

    *first_d++ = current;
    current = {};

    // subsequent



    auto next_source = [&first_s, &current, &start]
    {
        current = *first_s++;
        auto adjust = (std::min)(current.size(), start);
        start -= adjust;
        current += adjust;
    };

    while (first_s != last_s && first_d != last_d)
    {
        if (current.size() == 0)
        {
            current = *first_s++;
            if (current.size() == 0)
                continue;
        }

        auto adjust = (std::min)(current.size(), start);
        current += adjust;
        start -= adjust;
        if (current.size())
        *first_d = current;

    }

    return result;
}

template < class Executor,
           class MutableBufferSequence,
           class ReadStream,
           class WriteStream,
           class Handler >
struct copy_op
: asio::coroutine
, Handler
{
    MutableBufferSequence                 buf_;
    ReadStream                           &read_stream_;
    WriteStream                          &write_stream_;
    asio::executor_work_guard< Executor > wg_;
    copy_report                           report_;
    std::size_t to_write_

    copy_op(Executor              e,
            ReadStream           &rs,
            WriteStream          &ws,
            MutableBufferSequence buf,
            Handler               h)
    : buf_(buf)
    , read_stream_(rs)
    , write_stream_(ws)
    , wg_(std::move(e))
    , Handler(std::move(h))
    {
        (*this)();
    }

    void
    operator()(asio::error_code ec = asio::error_code(), std::size_t n = 0)
    {
        ASIO_CORO_REENTER(this)
        for (;;)
        {
            ASIO_CORO_YIELD
            read_stream_.async_read_some(buf_, asioex::notify_tick(*this));
            report_.read_bytes += n;
            report_.read_error = ec;

            if (ec && n == 0)
                return complete(ec, report_);

            ASIO_CORO_YIELD
            asio::async_write_until(write_stream_,
                                    buf_,
                                    asio::transfer_exactly(n),
                                    asioex::notify_tick(*this));
            total_ += n;

            if (ec)
                return complete(ec, total_);
        }
    }

    void
    complete(asio::error_code ec, std::size_t n)
    {
        auto wg = std::move(wg_);
        auto h  = std::move(static_cast< Handler >(*this));

        buf_.reset();
        asioex::invoke_or_post(wg.get_executor(), std::move(h));
    }
};

template < class ReadStream,
           class WriteStream,
           class MutableBufferSequence,
           ASIO_COMPLETION_HANDLER_FOR(void(asio::error_code, std::size_t))
               CopyHandler >
ASIO_INITFN_RESULT_TYPE(CopyHandler, void(asio::error_code, std::size_t))
async_copy(ReadStream           &rs,
           WriteStream          &ws,
           MutableBufferSequence buf,
           CopyHandler         &&token)
{
    return asio::async_initiate< CopyHandler,
                                 void(asio::error_code, std::size_t) >(
        [&rs, &ws, buf]< class Handler >(Handler &&handler)
        {
            auto h2 = auto e = asio::get_associated_executor(
                handler,
                asio::get_associated_executor(
                    rs, asio::get_associated_executor(ws)));

            using e_type = decltype(e);

            using op_type = copy_op< decltype(e),
                                     ReadStream,
                                     WriteStream,
                                     MutableBufferSequence,
                                     std::decay_t< Handler > >;
            op_type(
                std::move(e), rs, ws, buf, std::forward< Handler >(handler));
        },
        token);
}

int
main()
{
    auto handler = [](asioex::error_code) {};
    assert(!asioex::has_tick_state_v< decltype(handler) >);

    struct enabled_handler
    {
        bool &
        get_tick_state()
        {
            return tick_state_;
        }

        bool tick_state_;
    };

    assert(asioex::has_tick_state_v< enabled_handler >);

    auto h2 = asioex::enable_tick_state(std::move(handler));
    assert(asioex::has_tick_state_v< decltype(h2) >);
    auto h3 = asioex::enable_tick_state(enabled_handler());
    assert((std::is_same_v< decltype(h3), enabled_handler >));

    auto ioc = asio::io_context();
}