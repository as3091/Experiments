#ifndef DB_STUFF_H
#define DB_STUFF_H

bool db_init();
bool db_insert(const char* date_time, int raw);
bool db_delete(const char* date_time);
void db_clear();

typedef void (*db_row_cb)(const char* date_time, int reading);
void db_foreach(db_row_cb cb);
void db_print_all();

#endif
