#ifndef FISH_H
#define FISH_H

#include "types.h"

// Fish system initialization and cleanup
int fish_init(void);
void fish_cleanup(void);

// Fish management
int fish_add(float x, float y, int fish_type);
void fish_remove(int fish_id);

// SIMPLIFIED: Direct movement control (x, y direction)
void fish_set_movement_direction(int fish_id, float direction_x, float direction_y);
void fish_clear_movement_force(int fish_id);

// Fish eating and defecation functionality
void fish_eat_nearby_plants(int fish_id);
int fish_can_eat_plant(int fish_id, int node_id);
void fish_defecate(int fish_id);

// Enhanced vision & chemoreceptor system
void fish_update_vision(int fish_id);
void fish_cast_vision_ray(int fish_id, float angle, int ray_index);
void fish_cast_nutrition_ray(int fish_id, float angle, int ray_index);
void fish_update_chemoreceptors(int fish_id);

// Enhanced RL State management
void fish_update_rl_state(int fish_id);
void fish_update_oxygen_system(int fish_id);
void fish_update_hunger_system(int fish_id);
void fish_calculate_environmental_rewards(int fish_id);
void fish_calculate_chemoreceptor_rewards(int fish_id);

// SIMPLIFIED: Apply RL action - only direction, speed is constant
void fish_apply_rl_action(int fish_id, float direction_x, float direction_y);
float fish_get_reward(int fish_id);

// Enhanced RL State accessors for Python
float fish_get_vision_ray(int fish_id, int ray_index);
float fish_get_nutrition_ray(int fish_id, int ray_index);
float fish_get_oxygen_level(int fish_id);
float fish_get_hunger_level(int fish_id);
float fish_get_saturation_level(int fish_id);
float fish_get_last_reward(int fish_id);

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

// Ray rendering toggle
void fish_toggle_ray_rendering(void);
int fish_is_ray_rendering_enabled(void);

// Nutrition cycle tracking
float fish_get_total_nutrition_consumed(void);
float fish_get_total_nutrition_defecated(void);
float fish_get_nutrition_balance(void);

#endif // FISH_H