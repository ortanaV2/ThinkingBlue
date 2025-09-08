#ifndef FISH_H
#define FISH_H

#include "types.h"

// Fish system initialization and cleanup
int fish_init(void);
void fish_cleanup(void);

// Fish management
int fish_add(float x, float y, int fish_type);
void fish_remove(int fish_id);

// Fish control (called from Python)
void fish_set_movement_force(int fish_id, float force_x, float force_y);
void fish_clear_movement_force(int fish_id);

// Fish state queries
int fish_get_count(void);
Fish* fish_get_by_id(int fish_id);
Fish* fish_get_all(void);

// Fish update (called each frame)
void fish_update(void);

// Fish type management
int fish_load_config(const char* filename);
int fish_get_type_count(void);
FishType* fish_get_type(int index);

#endif // FISH_H