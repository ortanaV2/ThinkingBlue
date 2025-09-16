#ifndef FISH_H
#define FISH_H

#include "types.h"

// ============================================================================
// FISH_CORE.C - Core system management
// ============================================================================

// System initialization and cleanup
int fish_init(void);
void fish_cleanup(void);

// Configuration loading
int fish_load_config(const char* filename);

// Fish management
int fish_add(float x, float y, int fish_type);
void fish_remove(int fish_id);

// Type management
int fish_get_type_count(void);
FishType* fish_get_type(int index);

// State queries
int fish_get_count(void);
Fish* fish_get_by_id(int fish_id);
Fish* fish_get_all(void);

// Ray rendering toggle (legacy)
void fish_toggle_ray_rendering(void);
int fish_is_ray_rendering_enabled(void);

// Nutrition cycle tracking
float fish_get_total_nutrition_consumed(void);
float fish_get_total_nutrition_defecated(void);
float fish_get_nutrition_balance(void);

// Internal access for other fish modules
Fish* fish_internal_get_array(void);
FishType* fish_internal_get_types(void);
void fish_internal_add_consumed_nutrition(float amount);
void fish_internal_add_defecated_nutrition(float amount);

// ============================================================================
// FISH_VISION.C - RL vision and input system
// ============================================================================

// RL input system
void fish_update_rl_inputs(int fish_id);
float fish_get_distance_to_nearest_plant(int fish_id);

// Legacy vision functions (compatibility)
void fish_update_vision(int fish_id);
void fish_cast_vision_ray(int fish_id, float angle, int ray_index);
void fish_cast_nutrition_ray(int fish_id, float angle, int ray_index);
void fish_update_chemoreceptors(int fish_id);
float fish_get_vision_ray(int fish_id, int ray_index);
float fish_get_nutrition_ray(int fish_id, int ray_index);

// ============================================================================
// FISH_BEHAVIOR.C - RL behavior system
// ============================================================================

// RL control system
void fish_apply_rl_outputs(int fish_id);
void fish_calculate_rl_rewards(int fish_id);

// Eating system
int fish_attempt_eating(int fish_id);
void fish_defecate(int fish_id);

// RL state accessors for Python API
float fish_get_rl_input(int fish_id, int input_index);
void fish_set_rl_output(int fish_id, int output_index, float value);
float fish_get_last_reward(int fish_id);

// Legacy behavior functions (compatibility)
void fish_set_movement_force(int fish_id, float force_x, float force_y);
void fish_clear_movement_force(int fish_id);
void fish_apply_rl_action(int fish_id, float direction_x, float direction_y);
void fish_update_oxygen_system(int fish_id);
void fish_update_hunger_system(int fish_id);
void fish_update_rl_state(int fish_id);
void fish_calculate_environmental_rewards(int fish_id);
void fish_calculate_chemoreceptor_rewards(int fish_id);
float fish_get_reward(int fish_id);
void fish_eat_nearby_plants(int fish_id);
int fish_can_eat_plant(int fish_id, int node_id);
float fish_get_oxygen_level(int fish_id);
float fish_get_hunger_level(int fish_id);
float fish_get_saturation_level(int fish_id);

// ============================================================================
// FISH_UPDATE.C - Main update loop
// ============================================================================

// Main update function (called each frame)
void fish_update(void);

#endif // FISH_H