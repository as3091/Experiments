#include <Arduino.h>
#include "soil_sensor.h"
#include "db_stuff.h"
#include "secrets.h"

void take_reading(struct tm &time_now) {
    char date_time_now_string[15];
    strftime(date_time_now_string, sizeof(date_time_now_string), "%Y%m%d%H%M%S", &time_now);
    int raw = analogRead(SOIL_SENSOR_PIN);
    Serial.printf("%s : %d | Approx moisture: higher=dry\n", date_time_now_string, raw);
    db_insert(date_time_now_string, raw);
}
