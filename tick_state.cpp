#include <asioex/tick_state.hpp>
#include <asioex/error_code.hpp>

#include <assert.h>

int
main()
{
    auto handler = [](asioex::error_code) {};
    assert(!asioex::has_tick_state_v< decltype(handler)>);

    struct enabled_handler
    {

        bool& get_tick_state()
        {
            return tick_state_;
        }

        bool tick_state_;
    };

    assert(asioex::has_tick_state_v< enabled_handler>);

    auto h2 = asioex::enable_tick_state(std::move(handler));
    assert(asioex::has_tick_state_v<decltype(h2)>);
    auto h3 = asioex::enable_tick_state(enabled_handler());
    assert((std::is_same_v<decltype(h3), enabled_handler>));
}