#include "HealthTracker.h"

HealthTracker::HealthTracker(uint32_t level, uint32_t start_time_stamp, uint32_t time_to_discharge, uint32_t time_to_recharge) {
    this->level = level;
    this->last_tick_timestamp = start_time_stamp;
    this->time_to_discharge = time_to_discharge;
    this->time_to_recharge = time_to_recharge;
}

uint32_t HealthTracker::Tick(bool was_sitting_moment_ago, bool is_sitting, uint32_t now_timestamp) {
    if (was_sitting_moment_ago == is_sitting) {
        uint32_t tick_delta = now_timestamp - last_tick_timestamp;
        if (is_sitting) {
            level = max(0.0f, level - 100.0f * tick_delta / (float) time_to_discharge);
        } else {
            level = min(100.0f, level + 100.0f * tick_delta / (float) time_to_recharge);
        }
    }
    last_tick_timestamp = now_timestamp;
    return (uint32_t) level;
}
