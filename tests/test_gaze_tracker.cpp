#include "doctest.h"
#include <cstdint>

TEST_CASE("GazeTracker - Reference Counting Lifecycle Logic")
{
    int active_trackers = 0;
    bool is_processing = false;

    auto start_tracking = [&]() {
        active_trackers++;
        if (active_trackers == 1) {
            is_processing = true;
        }
    };

    auto stop_tracking = [&]() {
        if (active_trackers > 0) {
            active_trackers--;
            if (active_trackers == 0) {
                is_processing = false;
            }
        }
    };

    // Initial state
    CHECK(active_trackers == 0);
    CHECK_FALSE(is_processing);

    // First node enters tree
    start_tracking();
    CHECK(active_trackers == 1);
    CHECK(is_processing);

    // Second node enters tree (e.g. subscene or HUD)
    start_tracking();
    CHECK(active_trackers == 2);
    CHECK(is_processing);

    // First node exits tree
    stop_tracking();
    CHECK(active_trackers == 1);
    CHECK(is_processing);

    // Second node exits tree
    stop_tracking();
    CHECK(active_trackers == 0);
    CHECK_FALSE(is_processing);

    // Excess stop_tracking calls guard against underflow
    stop_tracking();
    CHECK(active_trackers == 0);
    CHECK_FALSE(is_processing);
}
