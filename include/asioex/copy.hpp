// Copyright (c) 2021 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef ASIO_EXPERIMENTS_COPY_HPP
#define ASIO_EXPERIMENTS_COPY_HPP

#include <asio/compose.hpp>
#include <asio/experimental/prepend.hpp>

namespace asioex
{

template<typename SourceOp, typename SinkOp>
struct async_copy_op
{
    SourceOp source;
    SinkOp sink;

    std::size_t completed = 0u;
    struct source_tag{};
    struct sink_tag{};

    template<typename Self>
    void operator()(Self && self)
    {
        completed --;
        source(asio::experimental::prepend(std::move(self), source_tag{}));
    }

    template<typename Self, typename ... Args>
    void operator()(Self && self, source_tag, asio::error_code ec, Args && ... args)
    {
        completed++;
        if (!ec && self.get_cancellation_state().cancelled() == asio::cancellation_type::terminal)
            ec = asio::error::operation_aborted;

        if (!ec)
            sink(std::forward<Args>(args)...,
                asio::experimental::prepend(std::move(self), sink_tag{}));
        else
            self.complete(ec, completed);
    }

    template<typename Self, typename ... Args>
    void operator()(Self && self, sink_tag, asio::error_code ec, Args && ... )
    {
        if (!ec && self.get_cancellation_state().cancelled() != asio::cancellation_type::none)
            ec = asio::error::operation_aborted;
        if (!ec)
            source(asio::experimental::prepend(std::move(self), source_tag{}));
        else
            self.complete(ec, completed);
    }
};

template<typename SourceOp, typename SinkOp, typename CompletionToken>
auto async_copy(SourceOp && source, SinkOp && sink, CompletionToken && token)
{
    return asio::async_compose<CompletionToken, void(std::error_code, std::size_t)>(
                    async_copy_op<SourceOp, SinkOp>{
                        std::forward<SourceOp>(source),
                        std::forward<SinkOp>(sink)
                    }, token);

}

}

#endif   // ASIO_EXPERIMENTS_COPY_HPP
