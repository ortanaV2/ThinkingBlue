#ifndef FISH_H
#define FISH_H

#include "types.h"

// Fish system initialization and cleanup
int fish_init(void);
void fish_cleanup(void);

// Fish management
int fish_add(float x, float y, int fish_type);
void fish_update(void);

// Player fish control
void fish_set_player_control(int keys[4]);
void fish_player_eat(void);

// Fish data access
Fish* fish_get_all(void);
int fish_get_count(void);

// Fish type management
int fish_load_config(const char* filename);
FishType* fish_get_type(int index);
int fish_get_type_count(void);

#endif // FISH_H