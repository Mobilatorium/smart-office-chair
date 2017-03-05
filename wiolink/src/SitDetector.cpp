#include "SitDetector.h"

SitDetector::SitDetector(Ultrasonic* ultrasonic, uint32_t threshold_distance_cm) {
    this->ultrasonic = ultrasonic;
    this->threshold_distance_cm = threshold_distance_cm;
}

bool SitDetector::IsSitting(bool was_sitting_moment_ago) {
    counter += (ultrasonic->MeasureInCentimeters() < threshold_distance_cm) ? +1 : -1;

    if (counter < COUNTER_DECISION_STAND) {
        counter = COUNTER_DECISION_STAND;
        return false;
    }

    if (counter > COUNTER_DECISION_SIT) {
        counter = COUNTER_DECISION_SIT;
        return true;
    }

    return was_sitting_moment_ago;
}
