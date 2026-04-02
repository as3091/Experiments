#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include "time.h"

bool soil_db_init();
bool soil_db_insert(struct tm &timeinfo, int raw);
void take_reading(struct tm &time_now);

typedef void (*soil_row_cb)(const char* date, const char* time_str, int reading);
void soil_db_foreach(soil_row_cb cb);
void soil_db_clear();

#endif
