#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <SDL2/SDL.h>

// Temperature system initialization and cleanup
int temperature_init(void);
void temperature_cleanup(void);

// Temperature management
float temperature_get_current(void);
void temperature_set_current(float temp);

// Coral bleaching system
void temperature_process_coral_bleaching(void);
int temperature_is_coral_bleached(int node_id);

// Stats file integration
void temperature_write_to_stats_file(FILE* stats_file);

#endif // TEMPERATURE_H