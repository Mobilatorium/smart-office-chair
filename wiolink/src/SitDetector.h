#ifndef SitDetector_h
#define SitDetector_h

#include <Arduino.h>
#include <Ultrasonic.h>

#define COUNTER_DECISION_STAND -5
#define COUNTER_DECISION_SIT 5

class SitDetector {

    public:
        SitDetector(Ultrasonic* ultrasonic, uint32_t threshold_distance_cm);
        bool IsSitting(bool was_sitting_moment_ago);

    private:
        Ultrasonic* ultrasonic;
        uint32_t threshold_distance_cm;
        int32_t counter = 0;

};
#endif
