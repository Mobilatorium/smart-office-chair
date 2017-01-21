#include <Arduino.h>
#include <Ultrasonic.h>
#include <Grove_LED_Bar.h>
#include <FirebaseArduino.h>
#include <ESP8266WiFi.h>
#include "SitDetector.h"
#include "HealthTracker.h"
#include "Config.h"

Grove_LED_Bar* led_bar;
SitDetector* sit_detector;
HealthTracker* health_tracker;
bool is_sitting;
bool online_mode;

void setup() {
    Serial.begin(28800);

    while (!Serial) {
        // wait serial port initialization
    }

    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);

    online_mode = true;
    uint32_t time_to_discharge_ms = DEFAULT_TIME_TO_DISCHARGE_MS;
    uint32_t time_to_recharge_ms = DEFAULT_TIME_TO_RECHARGE_MS;
    uint32_t threshold_distance_cm = DEFAULT_THRESHOLD_DISTANCE_CM;

    uint32_t wifi_start_ms = millis();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[INFO]  WiFi: connecting...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (millis() > (wifi_start_ms + WAITING_LIMIT_FOR_WIFI_CONNECTION_MS)) {
            online_mode = false;
            Serial.println("[ERROR] Cannot connect to WiFi. Will start in offline mode.");
            break;
        }
    }

    if (online_mode) {

        Firebase.begin(FIREBASE_HOST, FIREBASE_SECRET);

        Serial.println("[INFO]  Firebase: retrieving parameters...");
        time_to_discharge_ms = Firebase.getInt("config/time_to_discharge_ms");
        time_to_recharge_ms = Firebase.getInt("config/time_to_recharge_ms");
        threshold_distance_cm = Firebase.getInt("config/threshold_distance_cm");

        StaticJsonBuffer<200> timestamp_json_buffer;
        JsonObject& timestamp_json = timestamp_json_buffer.createObject();
        timestamp_json.set(".sv", "timestamp");

        StaticJsonBuffer<200> event_json_buffer;
        JsonObject& event_json = event_json_buffer.createObject();
        event_json.set("timestamp", timestamp_json);
        event_json.set("action", "connected");
        Firebase.push("/events", event_json);

        if (Firebase.failed()) {
            Serial.print("[ERROR] Firebase failed. ");
            Serial.println(Firebase.error());
            Serial.println("[INFO]  Will start in offline mode");
            online_mode = false;
        } if (time_to_discharge_ms == 0 || time_to_recharge_ms == 0 || threshold_distance_cm == 0) {
            Serial.println("[ERROR] Invalid paramaters. Will start in offline mode with default parameter values.");
            time_to_discharge_ms = DEFAULT_TIME_TO_DISCHARGE_MS;
            time_to_recharge_ms = DEFAULT_TIME_TO_RECHARGE_MS;
            threshold_distance_cm = DEFAULT_THRESHOLD_DISTANCE_CM;
            online_mode = false;
        } else {
            Serial.println("[INFO]  Successfully conneceted to WiFi and Firebase. Will start in online mode");
        }
    }

    sit_detector = new SitDetector(new Ultrasonic(PIN_ULTRASONIC_SIG), threshold_distance_cm);
    health_tracker = new HealthTracker(millis(), time_to_discharge_ms, time_to_recharge_ms);

    led_bar = new Grove_LED_Bar(PIN_LED_BAR_DCKI, PIN_LED_BAR_DI, 0);
    led_bar->begin();

    Serial.println("[INFO]  Setup has been finished.");
}

void loop() {

    uint32_t now_timestamp_ms = millis();

    bool was_sitting_moment_ago = is_sitting;
    is_sitting = sit_detector->IsSitting();

    float health = health_tracker->Tick(was_sitting_moment_ago, is_sitting, now_timestamp_ms);
    float health_bar_level = abs(health) < 1e-10 ? 0.5f : health * 10.f;

    Serial.print("[INFO]  now_timestamp_ms = ");
    Serial.print(now_timestamp_ms);
    Serial.print(", is_sitting = ");
    Serial.print(is_sitting);
    Serial.print(", health = ");
    Serial.println(health);

    led_bar->setLevel(health_bar_level);

    if (online_mode && was_sitting_moment_ago != is_sitting) {

        StaticJsonBuffer<200> timestamp_json_buffer;
        JsonObject& timestamp_json = timestamp_json_buffer.createObject();
        timestamp_json.set(".sv", "timestamp");

        StaticJsonBuffer<200> event_json_buffer;
        JsonObject& event_json = event_json_buffer.createObject();
        event_json.set("timestamp", timestamp_json);
        event_json.set("health", health);
        event_json.set("action", is_sitting ? "sit" : "stand");
        Firebase.push("/events", event_json);
    }
}
