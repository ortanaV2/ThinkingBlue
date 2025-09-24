#ifndef FISH_H
#define FISH_H

#include "types.h"

// ============================================================================
// FISH_CORE.C - Core system management with robust ID tracking
// ============================================================================

// System initialization and cleanup
int fish_init(void);
void fish_cleanup(void);

// Configuration loading
int fish_load_config(const char* filename);

// Fish management with robust ID system
int fish_add(float x, float y, int fish_type);
void fish_remove(int fish_id);

// Aging system
int fish_should_die_from_age(int fish_id);
int fish_get_total_deaths_from_age(void);

// Corpse system
int fish_get_total_corpses_created(void);
int fish_get_total_corpses_eaten(void);
void fish_increment_corpses_eaten(void);

// Type management
int fish_get_type_count(void);
FishType* fish_get_type(int index);

// State queries with robust ID handling
int fish_get_count(void);                    // Returns active fish count
int fish_get_highest_slot(void);             // Returns highest used slot for efficient iteration
Fish* fish_get_by_id(int fish_id);          // Validated fish access
Fish* fish_get_all(void);                   // Returns fish array

// Ray rendering toggle
void fish_toggle_ray_rendering(void);
int fish_is_ray_rendering_enabled(void);

// Nutrition cycle tracking
float fish_get_total_nutrition_consumed(void);
float fish_get_total_nutrition_defecated(void);
float fish_get_nutrition_balance(void);

// Debug function
void fish_debug_print_status(void);

// Internal access for other fish modules
Fish* fish_internal_get_array(void);
FishType* fish_internal_get_types(void);
void fish_internal_add_consumed_nutrition(float amount);
void fish_internal_add_defecated_nutrition(float amount);

// ============================================================================
// FISH_VISION.C - Enhanced RL vision with predator detection
// ============================================================================

// RL input system with predator detection
void fish_update_rl_inputs(int fish_id);
float fish_get_distance_to_nearest_plant(int fish_id);
float fish_get_distance_to_nearest_foreign_fish(int fish_id);

// Legacy vision functions (compatibility)
void fish_update_vision(int fish_id);
void fish_cast_vision_ray(int fish_id, float angle, int ray_index);
void fish_cast_nutrition_ray(int fish_id, float angle, int ray_index);
void fish_update_chemoreceptors(int fish_id);
float fish_get_vision_ray(int fish_id, int ray_index);
float fish_get_nutrition_ray(int fish_id, int ray_index);

// ============================================================================
// FISH_BEHAVIOR.C - Enhanced RL behavior with predator-prey system and corpse eating
// ============================================================================

// RL control system
void fish_apply_rl_outputs(int fish_id);
void fish_calculate_rl_rewards(int fish_id);

// Enhanced eating system
int fish_attempt_eating(int fish_id);           // Wrapper function
int fish_attempt_eating_plant(int fish_id);     // Herbivore eating
int fish_attempt_eating_fish(int fish_id);      // Predator eating
int fish_attempt_eating_corpse(int fish_id);    // Corpse eating
void fish_defecate(int fish_id);

// Reproduction system with inheritance support
void fish_reproduce(int fish_id);
void fish_predator_reproduce(int fish_id);

// Neural network inheritance support
int fish_get_parent_for_inheritance(void);
int fish_is_reproduction_pending(void);

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
// FISH_UPDATE.C - Main update loop with robust ID handling
// ============================================================================

// Main update function (called each frame)
void fish_update(void);

// Corpse system functions
void fish_update_corpses(void);

#endif // FISH_H