#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include "time.h"

bool soil_db_init();
bool soil_db_insert(const char* date_time, int raw);
void take_reading(struct tm &time_now);

typedef void (*soil_row_cb)(const char* date_time, int reading);
void soil_db_foreach(soil_row_cb cb);
void soil_db_print_all();
bool soil_db_delete(const char* date_time);
void soil_db_clear();

#endif
