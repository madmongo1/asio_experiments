//
// Copyright (c) 2021 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/madmongo1/async_experiments
//

#ifndef ASIO_EXPERIMENTS_FILING_CABINET_EXPANDING_CIRCULAR_BUFFER_HPP
#define ASIO_EXPERIMENTS_FILING_CABINET_EXPANDING_CIRCULAR_BUFFER_HPP

#include <cstddef>

namespace asio::detail
{
template < class T >
struct expanding_circular_buffer
{
    static constexpr std::size_t initial_capacity = 16;

    void
    push(T p)
    {
        if (size_ == capacity_)
        {
            if (!storage_)
                init();
            else
                grow();
        }
        size_ += 1;
        storage_[back_pos_] = p;
        if (++back_pos_ >= capacity_)
            back_pos_ -= capacity_;
    }

    T
    pop()
    {
        assert(size_);
        auto result = storage_[front_pos_];
        if (++front_pos_ >= capacity_)
            front_pos_ -= capacity_;
        size_ -= 1;
        return result;
    }

    std::size_t
    size() const
    {
        return size_;
    }

  private:
    void
    init()
    {
        storage_   = std::make_unique< T[] >(initial_capacity);
        capacity_  = initial_capacity;
        front_pos_ = 0;
        back_pos_  = 0;
    }

    void
    grow()
    {
        if (capacity_ > std::numeric_limits< std::size_t >::max() / 2)
            throw std::bad_alloc();
        auto new_cap     = capacity_ * 2;
        auto new_storage = std::make_unique< T[] >(new_cap);

        std::size_t size = 0;

        // the front is ahead of the back (split buffer)
        if (back_pos_ <= front_pos_)
        {
            auto first = &new_storage[0];
            auto last =
                std::copy(&storage_[front_pos_], &storage_[capacity_], first);
            last = std::copy(&storage_[0], &storage_[back_pos_], last);
            size = std::distance(first, last);
            assert(size == size_);
        }
        else
        {
            auto first = &new_storage[0];
            auto last =
                std::copy(&storage_[front_pos_], &storage_[back_pos_], first);
            size = std::distance(first, last);
            assert(size == size_);
        }
        storage_   = std::move(new_storage);
        capacity_  = new_cap;
        front_pos_ = 0;
        back_pos_  = size;
    }

    std::size_t            capacity_  = 0;
    std::size_t            size_      = 0;
    std::size_t            front_pos_ = 0;
    std::size_t            back_pos_  = 0;
    std::unique_ptr< T[] > storage_;
};

}   // namespace asio::detail

#endif   // ASIO_EXPERIMENTS_FILING_CABINET_EXPANDING_CIRCULAR_BUFFER_HPP
