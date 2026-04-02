#ifndef LISTENER_H
#define LISTENER_H

#include "time.h"

void setup_listener();
void process_loop();
struct tm get_next_listen_time(struct tm time_now);
struct tm mqtt_check(struct tm time_now);

#endif