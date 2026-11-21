#include <assert.h>
#include <stdint.h>
#include "host_rate_window.h"

int main(void)
{
    host_rate_window_t window = {0};
    uint32_t rate = 123;
    assert(!host_rate_window_update(&window, UINT32_MAX - 100000u, UINT32_MAX - 5000u, false, &rate));
    assert(rate == 0);
    assert(!host_rate_window_update(&window, UINT32_MAX - 1u, UINT32_MAX - 200u, true, &rate));
    assert(rate == 0);
    assert(host_rate_window_update(&window, 149999u, 6999u, true, &rate));
    assert(rate == 48000u);
    assert(!host_rate_window_update(&window, 200000u, 9400u, true, &rate));
    assert(rate == 48000u);
    assert(host_rate_window_update(&window, 399999u, 18999u, true, &rate));
    assert(rate == 48000u);
    assert(!host_rate_window_update(&window, 400000u, 19000u, false, &rate));
    assert(rate == 0);
    assert(host_rate_window_update(&window, 650000u, 30025u, true, &rate));
    assert(rate == 44100u);
    return 0;
}
