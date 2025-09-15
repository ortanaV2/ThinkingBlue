// fish_behavior.c - Fish eating, movement, and RL behavior
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"
#include "grid.h"
#include "plants.h"
#include "nutrition.h"
#include "gas.h"

// Calculate plant nutrition cost
static float calculate_plant_nutrition_cost(int plant_type) {
    if (plant_type < 0 || plant_type >= plants_get_type_count()) {
        return 0.1f;
    }
    
    PlantType* pt = plants_get_type(plant_type);
    if (!pt) return 0.1f;
    
    float size_factor = (pt->max_branches / 3.0f) * (pt->branch_distance / OPTIMAL_DISTANCE);
    float actual_nutrition_cost = pt->nutrition_depletion_strength * size_factor;
    
    return actual_nutrition_cost;
}

// Movement control
void fish_set_movement_force(int fish_id, float force_x, float force_y) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    // Normalize input to unit vector for consistent speed
    float magnitude = sqrt(force_x * force_x + force_y * force_y);
    if (magnitude > 0.1f) {
        fish->movement_force_x = force_x / magnitude;
        fish->movement_force_y = force_y / magnitude;
    } else {
        fish->movement_force_x = 0.0f;
        fish->movement_force_y = 0.0f;
    }
}

void fish_clear_movement_force(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    fish->movement_force_x = 0.0f;
    fish->movement_force_y = 0.0f;
}

// RL action application
void fish_apply_rl_action(int fish_id, float direction_x, float direction_y) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    // Clamp direction inputs
    if (direction_x < -1.0f) direction_x = -1.0f;
    if (direction_x > 1.0f) direction_x = 1.0f;
    if (direction_y < -1.0f) direction_y = -1.0f;
    if (direction_y > 1.0f) direction_y = 1.0f;
    
    // Normalize to unit vector for consistent movement speed
    float magnitude = sqrt(direction_x * direction_x + direction_y * direction_y);
    if (magnitude > 0.1f) {
        direction_x /= magnitude;
        direction_y /= magnitude;
    } else {
        direction_x = 0.0f;
        direction_y = 0.0f;
    }
    
    fish->movement_force_x = direction_x;
    fish->movement_force_y = direction_y;
    fish->desired_speed = 1.0f;
}

// Oxygen system
void fish_update_oxygen_system(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float environmental_oxygen = gas_get_oxygen_at(fish_node->x, fish_node->y);
    
    // Reduced oxygen consumption
    fish->oxygen_level -= fish_type->oxygen_consumption_rate * 0.5f;
    
    // Enhanced oxygen refill
    if (environmental_oxygen > 0.1f) {
        float refill_rate = fish_type->oxygen_refill_rate * 2.0f * (environmental_oxygen + 0.2f);
        fish->oxygen_level += refill_rate;
    }
    
    if (fish->oxygen_level < 0.0f) fish->oxygen_level = 0.0f;
    if (fish->oxygen_level > 1.0f) fish->oxygen_level = 1.0f;
}

// Hunger system
void fish_update_hunger_system(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    fish->hunger_level += 0.0003f;
    
    if (fish->hunger_level < 0.0f) fish->hunger_level = 0.0f;
    if (fish->hunger_level > 1.0f) fish->hunger_level = 1.0f;
    
    fish->saturation_level = fish->stomach_contents;
    if (fish->saturation_level > 1.0f) fish->saturation_level = 1.0f;
}

// RL state management
void fish_update_rl_state(int fish_id) {
    fish_update_vision(fish_id);
    fish_update_chemoreceptors(fish_id);
    fish_update_oxygen_system(fish_id);
    fish_update_hunger_system(fish_id);
}

// Environmental rewards
void fish_calculate_environmental_rewards(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    float oxygen_penalty = 0.0f;
    if (fish->oxygen_level < 0.6f) {
        float oxygen_deficit = 0.6f - fish->oxygen_level;
        oxygen_penalty = oxygen_deficit * oxygen_deficit * 0.05f;
    }
    
    float hunger_penalty = fish->hunger_level * 0.02f;
    
    fish->last_reward -= oxygen_penalty;
    fish->last_reward -= hunger_penalty;
    
    if (fish->oxygen_level > 0.8f && fish->hunger_level < 0.3f) {
        fish->last_reward += 0.005f;
    }
}

// Chemoreceptor rewards
void fish_calculate_chemoreceptor_rewards(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    float total_nutrition_detected = 0.0f;
    float max_ray_nutrition = 0.0f;
    
    for (int i = 0; i < CHEMORECEPTOR_RAYS; i++) {
        total_nutrition_detected += fish->nutrition_rays[i];
        if (fish->nutrition_rays[i] > max_ray_nutrition) {
            max_ray_nutrition = fish->nutrition_rays[i];
        }
    }
    
    if (total_nutrition_detected > 0.1f) {
        float detection_reward = total_nutrition_detected * 0.02f;
        fish->last_reward += detection_reward;
        
        if (max_ray_nutrition > 0.5f) {
            fish->last_reward += 0.01f;
        }
        
        if (fish->hunger_level > 0.5f) {
            fish->last_reward += total_nutrition_detected * fish->hunger_level * 0.01f;
        }
    }
}

// Check if fish can eat plant
int fish_can_eat_plant(int fish_id, int node_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    Node* nodes = simulation_get_nodes();
    if (node_id < 0 || node_id >= simulation_get_node_count()) return 0;
    if (!nodes[node_id].active) return 0;
    if (nodes[node_id].plant_type == -1) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return 0;
    
    Node* fish_node = &nodes[fish->node_id];
    Node* plant_node = &nodes[node_id];
    
    float dx = plant_node->x - fish_node->x;
    float dy = plant_node->y - fish_node->y;
    float distance = sqrt(dx * dx + dy * dy);
    
    return distance <= fish_type->eating_range;
}

// Eating system
void fish_eat_nearby_plants(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    
    int current_frame = simulation_get_frame_counter();
    float eating_chance = fish_type->eating_rate;
    
    if ((float)rand() / RAND_MAX > eating_chance) {
        return;
    }
    
    Node* fish_node = &nodes[fish->node_id];
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    
    GridCell* cells[9];
    int cell_count = grid_get_cells_at_position(fish_x, fish_y, cells, 9);
    
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    
    for (int c = 0; c < cell_count; c++) {
        GridCell* cell = cells[c];
        if (!cell) continue;
        
        for (int k = 0; k < cell->count; k++) {
            int node_id = cell->node_indices[k];
            if (node_id < 0 || node_id >= simulation_get_node_count()) continue;
            if (!nodes[node_id].active) continue;
            if (nodes[node_id].plant_type == -1) continue;
            
            float dx = nodes[node_id].x - fish_x;
            float dy = nodes[node_id].y - fish_y;
            float distance_sq = dx * dx + dy * dy;
            
            if (distance_sq <= eating_range_sq) {
                int plant_type = nodes[node_id].plant_type;
                PlantType* pt = plants_get_type(plant_type);
                if (!pt) continue;
                
                float plant_nutrition = calculate_plant_nutrition_cost(plant_type);
                
                fish->stomach_contents += plant_nutrition;
                fish->hunger_level -= plant_nutrition * 0.8f;
                if (fish->hunger_level < 0.0f) fish->hunger_level = 0.0f;
                
                fish_internal_add_consumed_nutrition(plant_nutrition);
                
                // Enhanced nutrition-based rewards
                float base_nutrition_reward = plant_nutrition * 15.0f;
                
                float nutrition_bonus = 0.0f;
                if (plant_nutrition > 0.4f) {
                    nutrition_bonus = (plant_nutrition - 0.4f) * 20.0f;
                } else if (plant_nutrition < 0.2f) {
                    nutrition_bonus = -0.1f;
                }
                
                float hunger_bonus = fish->hunger_level * 0.1f;
                
                float total_reward = base_nutrition_reward + nutrition_bonus + hunger_bonus;
                fish->last_reward += total_reward;
                fish->total_reward += total_reward;
                
                fish->last_eating_frame = current_frame;
                
                // Remove eaten plant
                nodes[node_id].active = 0;
                nodes[node_id].can_grow = 0;
                
                return;
            }
        }
    }
    
    // Reduced penalty for not finding food
    if (fish->hunger_level > 0.7f) {
        fish->last_reward -= 0.015f;
    } else {
        fish->last_reward -= 0.005f;
    }
}

// Defecation system
void fish_defecate(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    if (fish->stomach_contents < 0.7f) {
        return;
    }
    
    float defecation_amount = fish->stomach_contents;
    
    nutrition_add_at_position(fish_node->x, fish_node->y, 
                              defecation_amount, fish_type->defecation_radius);
    
    fish->stomach_contents = 0.0f;
    fish_internal_add_defecated_nutrition(defecation_amount);
    
    // 30% chance to spawn a random seed at defecation location
    if ((float)rand() / RAND_MAX < 0.3f) {
        int plant_type_count = plants_get_type_count();
        if (plant_type_count > 0) {
            int random_plant_type = rand() % plant_type_count;
            
            float seed_offset_x = ((float)rand() / RAND_MAX - 0.5f) * 20.0f;
            float seed_offset_y = ((float)rand() / RAND_MAX - 0.5f) * 20.0f;
            
            float seed_x = fish_node->x + seed_offset_x;
            float seed_y = fish_node->y + seed_offset_y;
            
            if (seed_x >= WORLD_LEFT && seed_x <= WORLD_RIGHT &&
                seed_y >= WORLD_TOP && seed_y <= WORLD_BOTTOM) {
                
                int new_node = simulation_add_node(seed_x, seed_y, random_plant_type);
                if (new_node >= 0) {
                    PlantType* pt = plants_get_type(random_plant_type);
                    printf("Fish %d planted %s seed at defecation spot\n", fish_id, pt->name);
                }
            }
        }
    }
}

// RL state accessors
float fish_get_oxygen_level(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->oxygen_level : 0.0f;
}

float fish_get_hunger_level(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->hunger_level : 0.0f;
}

float fish_get_saturation_level(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->saturation_level : 0.0f;
}

float fish_get_reward(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->last_reward : 0.0f;
}

float fish_get_last_reward(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->last_reward : 0.0f;
}