#include <Arduino.h>
#include <Ultrasonic.h>
#include <Grove_LED_Bar.h>
#include <FirebaseArduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266TrueRandom.h>
#include <EEPROM.h>
#include "SitDetector.h"
#include "HealthTracker.h"
#include "Config.h"

bool online_mode = true;
uint32_t time_to_discharge_ms = DEFAULT_TIME_TO_DISCHARGE_MS;
uint32_t time_to_recharge_ms = DEFAULT_TIME_TO_RECHARGE_MS;
uint32_t time_data_interval_ms = DEFAULT_TIME_DATA_INTERVAL_MS;
uint32_t threshold_distance_cm = DEFAULT_THRESHOLD_DISTANCE_CM;

Grove_LED_Bar* led_bar;
SitDetector* sit_detector;
HealthTracker* health_tracker;

bool is_sitting;
uint32_t health;
uint32_t send_wait_for_local_time_ms;
bool send_max_health_data;
String uuid;

void setup_serial() {
    Serial.begin(28800);

    while (!Serial) {
        // wait serial port initialization
    }

    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
}

bool read_uuid_from_EEPROM(byte* uuid_number) {
    bool is_valid_uuid = false;
    for (uint32_t i = 0; i < 16; ++i) {
        uuid_number[i] = EEPROM.read(i);
        if (uuid_number[i] != 0xff && uuid_number[i] != 0x00) {
            is_valid_uuid = true;
        }
    }
    return is_valid_uuid;
}

bool write_uuid_to_EEPROM(byte* uuid_number) {
    for (uint32_t i = 0; i < 16; ++i) {
        EEPROM.write(i, uuid_number[i]);
    }
    EEPROM.commit();
    byte commited_uuid_number[16];
    bool commited = read_uuid_from_EEPROM(commited_uuid_number);
    if (!commited) {
        return false;
    }
    for (uint32_t i = 0; i < 16; ++i) {
        if (uuid_number[i] != commited_uuid_number[i]) {
            return false;
        }
    }
    return true;
}

void setup_uuid() {
    EEPROM.begin(512);

    byte uuid_number[16];
    bool has_valid_uuid_stored = read_uuid_from_EEPROM(uuid_number);

    if (has_valid_uuid_stored) {
        uuid = ESP8266TrueRandom.uuidToString(uuid_number);
        Serial.print("[INFO] Has valid uuid stored = ");
        Serial.println(uuid);
    } else {
        ESP8266TrueRandom.uuid(uuid_number);
        bool has_written_new_uuid = write_uuid_to_EEPROM(uuid_number);
        if (has_written_new_uuid) {
            uuid = ESP8266TrueRandom.uuidToString(uuid_number);
            Serial.print("[INFO] Has associated new uuid = ");
            Serial.println(uuid);
        } else {
            uuid = "UNDEFINED";
            Serial.print("[INFO] Cannot setup uuid. Will use 'UNDEFINED'");
        }
    }
}

void setup_wi_fi() {
    online_mode = true;
    uint32_t wifi_start_ms = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[INFO]  WiFi: connecting...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (millis() > (wifi_start_ms + WAITING_LIMIT_FOR_WIFI_CONNECTION_MS)) {
            online_mode = false;
            Serial.println("[ERRR] Cannot connect to WiFi. Will start in offline mode.");
            break;
        }
    }
}

void setup_firebase() {
    if (!online_mode) {
        return;
    }

    Firebase.begin(FIREBASE_HOST, FIREBASE_SECRET);

    // read config
    Serial.println("[INFO]  Firebase: retrieving parameters...");

    time_to_discharge_ms = Firebase.getInt("config/time_to_discharge_ms");
    time_to_recharge_ms = Firebase.getInt("config/time_to_recharge_ms");
    time_data_interval_ms = Firebase.getInt("config/time_data_interval_ms");
    threshold_distance_cm = Firebase.getInt("config/threshold_distance_cm");

    if (Firebase.failed()) {
        Serial.print("[ERRR] Firebase failed. ");
        Serial.println(Firebase.error());
        Serial.println("[INFO] Will start in offline mode");
        online_mode = false;
        return;
    } if (time_to_discharge_ms == 0 || time_to_recharge_ms == 0 || time_data_interval_ms == 0 || threshold_distance_cm == 0) {
        Serial.println("[ERRR] Invalid paramaters. Will start in offline mode with default parameter values.");
        time_to_discharge_ms = DEFAULT_TIME_TO_DISCHARGE_MS;
        time_to_recharge_ms = DEFAULT_TIME_TO_RECHARGE_MS;
        time_data_interval_ms = DEFAULT_TIME_DATA_INTERVAL_MS;
        threshold_distance_cm = DEFAULT_THRESHOLD_DISTANCE_CM;
        online_mode = false;
        return;
    };

    // write connection timestamp
    StaticJsonBuffer<200> timestamp_json_buffer;
    JsonObject& timestamp_json = timestamp_json_buffer.createObject();
    timestamp_json.set(".sv", "timestamp");

    StaticJsonBuffer<200> event_json_buffer;
    JsonObject& event_json = event_json_buffer.createObject();
    event_json.set("timestamp", timestamp_json);
    Firebase.push(uuid + "/connections", event_json);

    if (Firebase.failed()) {
        Serial.print("[ERRR] Firebase write operation failed. ");
        Serial.println(Firebase.error());
        Serial.println("[INFO] Will start in offline mode with parameter values from Firebase.");
        online_mode = false;
        return;
    }

    // backup state from the latest data
    bool has_latest = Firebase.getInt(uuid + "/data/latest/timestamp") > 0;

    if (has_latest) {
        is_sitting = Firebase.getBool(uuid + "/data/latest/is_sitting");
        health = Firebase.getInt(uuid + "/data/latest/health");
        send_max_health_data = health < 100;
        Serial.println("[INFO] Restored the latest from Firebase.");
    } else {
        is_sitting = false;
        health = 100;
        send_max_health_data = false;
    }

}

void setup_sensors() {
    if (!online_mode) {
        is_sitting = false;
        health = 100;
    }
    sit_detector = new SitDetector(new Ultrasonic(PIN_ULTRASONIC_SIG), threshold_distance_cm);
    health_tracker = new HealthTracker(health, millis(), time_to_discharge_ms, time_to_recharge_ms);

    led_bar = new Grove_LED_Bar(PIN_LED_BAR_DCKI, PIN_LED_BAR_DI, 0);
    led_bar->begin();
}

void setup() {

    setup_serial();
    setup_uuid();
    setup_wi_fi();
    setup_firebase();
    setup_sensors();

    if (online_mode) {
        Serial.println("[INFO] Will start in online mode. Both WiFi and Firebase are working correctly.");
    }

}

void loop() {

    uint32_t local_time_ms = millis();

    bool was_sitting_moment_ago = is_sitting;
    is_sitting = sit_detector->IsSitting(was_sitting_moment_ago);
    health = health_tracker->Tick(was_sitting_moment_ago, is_sitting, local_time_ms);

    float led_bar_level = health / 10.0f;
    if (led_bar_level < 0.5f) {
        led_bar_level = (local_time_ms % 5000) < 4500 ? 0.5f : 0.0f;
    }
    led_bar->setLevel(led_bar_level);

    if (online_mode) {
        bool should_send_now =
            (is_sitting != was_sitting_moment_ago) ||
            (health == 100 && send_max_health_data) ||
            (health < 100 && local_time_ms > send_wait_for_local_time_ms);

        if (should_send_now) {
            StaticJsonBuffer<200> timestamp_json_buffer;
            JsonObject& timestamp_json = timestamp_json_buffer.createObject();
            timestamp_json.set(".sv", "timestamp");

            StaticJsonBuffer<200> event_json_buffer;
            JsonObject& event_json = event_json_buffer.createObject();
            event_json.set("timestamp", timestamp_json);
            event_json.set("health", health);
            event_json.set("is_sitting", is_sitting);
            Firebase.push(uuid + "/data/all", event_json);
            Firebase.set(uuid + "/data/latest", event_json);

            if (Firebase.success()) {
                Serial.print("[INFO] Saved to Firebase: is_sitting = ");
                Serial.print(is_sitting);
                Serial.print(", health = ");
                Serial.print(health);
                Serial.print(". Timestamp: ");
                Serial.println(local_time_ms);
                send_wait_for_local_time_ms = local_time_ms + time_data_interval_ms;
                send_max_health_data = health < 100;
            } else {
                Serial.printf("[ERRR] Cannot save to Firebase. Will retry shortly");
            }
        }
    } else {
        Serial.print("[INFO] Offline mode. is_sitting = ");
        Serial.print(is_sitting);
        Serial.print(", health = ");
        Serial.print(health);
        Serial.print(". Timestamp: ");
        Serial.println(local_time_ms);
        delay(250);
    }
}
