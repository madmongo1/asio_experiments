
#ifndef ASIOEX_TICK_STATE_HPP
#define ASIOEX_TICK_STATE_HPP

#include <asio/associated_executor.hpp>

#include <type_traits>

namespace asioex
{
template < class T >
struct test_has_tick_state
{
    // clang-format off
    template < class U >
    static auto
    test(U &) ->
        decltype(std::declval< U & >().get_tick_state(),
                 void(),
                 std::true_type());

    template < class U >
    static auto
    test(...) -> std::false_type;

    using type = decltype(test<T>(std::declval< T& >()));

    static constexpr
    bool value = type::value;
    // clang-format on
};

template < typename T >
using has_tick_state = typename test_has_tick_state< std::decay_t< T > >::type;

template < class T >
constexpr bool has_tick_state_v = has_tick_state< T >::value;

template < class Handler, typename = void >
struct associated_tick_state
{
};

template < class Handler >
struct associated_tick_state< Handler,
                              std::enable_if_t< has_tick_state_v< Handler > > >
{
    using type =
        std::decay_t< decltype(std::declval< Handler & >().get_tick_state()) >;
};

struct terminal_class
{
};

template < class Derived, class Base = terminal_class, typename = void >
struct wrap_executor_type : Base
{
};

template < class Derived, class Base >
struct wrap_executor_type<
    Derived,
    Base,
    std::enable_if_t< asio::detail::has_executor_type< Base >::value > > : Base
{
    using executor_type = typename Base::executor_type;

    executor_type
    get_executor()
    {
        return Base::get_executor();
    }
};

template < class Handler >
struct handler_enable_tick_state : Handler
{
    template <
        class HandlerArg,
        std::enable_if_t< !std::is_base_of_v< handler_enable_tick_state,
                                              std::decay_t< HandlerArg > > > * =
            nullptr >
    handler_enable_tick_state(HandlerArg &&arg)
    : Handler(std::forward< HandlerArg >(arg))
    {
    }

    bool &
    get_tick_state()
    {
        return tick_state_;
    }

  private:
    bool tick_state_ = false;
};

template < class Handler, typename = void >
struct select_tick_state_handler
{
    using type = handler_enable_tick_state< Handler >;
};

template < class Handler >
struct select_tick_state_handler<
    Handler,
    std::enable_if_t< has_tick_state_v< Handler > > >
{
    using type = Handler;
};

template < class Handler >
typename select_tick_state_handler< std::decay_t< Handler > >::type
enable_tick_state(Handler handler)
{
    using my_handler =
        typename select_tick_state_handler< std::decay_t< Handler > >::type;

    return my_handler(std::move(handler));
}

}   // namespace asioex

#endif
