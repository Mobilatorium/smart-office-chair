#include "SitDetector.h"

SitDetector::SitDetector(Ultrasonic* ultrasonic, uint32_t threshold_distance_cm) {
    this->ultrasonic = ultrasonic;
    this->threshold_distance_cm = threshold_distance_cm;
}

bool SitDetector::IsSitting() {
    int32_t counter = 0;
    for (int i = 0; i < sampling_number; ++i) {
        if (ultrasonic->MeasureInCentimeters() < threshold_distance_cm) {
            ++counter;
        }
    }
    return counter > sampling_number / 2;
}
