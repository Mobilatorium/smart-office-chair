#ifndef HealthTracker_h
#define HealthTracker_h

#include <Arduino.h>

class HealthTracker {

    public:
        HealthTracker(uint32_t level, uint32_t start_time_stamp, uint32_t time_to_discharge, uint32_t time_to_recharge);
        uint32_t Tick(bool was_sitting_moment_ago, bool is_sitting, uint32_t now_timestamp);

    private:
        uint32_t time_to_discharge, time_to_recharge, last_tick_timestamp;
        float level;

};
#endif
