#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include "time.h"

bool soil_db_init();
bool soil_db_insert(struct tm &timeinfo, int raw);

#endif
