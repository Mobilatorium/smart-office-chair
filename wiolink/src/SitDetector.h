#ifndef SitDetector_h
#define SitDetector_h

#include <Arduino.h>
#include <Ultrasonic.h>

class SitDetector {

    public:
        SitDetector(Ultrasonic* ultrasonic, uint32_t threshold_distance_cm);
        bool IsSitting();

    private:
        Ultrasonic* ultrasonic;
        uint32_t threshold_distance_cm;
        uint32_t sampling_number = 10;

};
#endif
