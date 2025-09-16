// fish_behaviour.c - RL-based fish behavior system (with debug)
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

// Calculate plant nutrition value
static float calculate_plant_nutrition_value(int plant_type) {
    if (plant_type < 0 || plant_type >= plants_get_type_count()) {
        return 0.1f;
    }
    
    PlantType* pt = plants_get_type(plant_type);
    if (!pt) return 0.1f;
    
    float size_factor = (pt->max_branches / 3.0f) * (pt->branch_distance / OPTIMAL_DISTANCE);
    float nutrition_value = pt->nutrition_depletion_strength * size_factor;
    
    return nutrition_value;
}

// Apply RL outputs to fish behavior - FORCE-BASED MOVEMENT
void fish_apply_rl_outputs(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    // Parse RL outputs
    float turn_direction = fish->rl_outputs[0];      // -1.0 to 1.0
    float movement_strength = fish->rl_outputs[1];   // 0.0 to 1.0
    float eat_command = fish->rl_outputs[2];         // 0.0 to 1.0
    
    // DEBUG: Check if we have any RL outputs at all
    static int debug_counters[MAX_FISH] = {0};
    if (fish_id < MAX_FISH && (debug_counters[fish_id]++ % 30) == 0) {  // Every 30 frames (1 second at 30fps)
        printf("DEBUG fish_apply_rl_outputs: Fish %d raw outputs = (%.3f, %.3f, %.3f)\n",
               fish_id, turn_direction, movement_strength, eat_command);
        
        // Check if outputs are all zero (indicating Python controller not working)
        if (turn_direction == 0.0f && movement_strength == 0.0f && eat_command == 0.0f) {
            printf("WARNING: Fish %d has zero RL outputs! Python controller may not be working.\n", fish_id);
            
            // Apply some default test behavior
            printf("Applying default test behavior to fish %d\n", fish_id);
            turn_direction = ((float)rand() / RAND_MAX - 0.5f) * 0.3f;  // Small random turn
            movement_strength = 0.5f;  // Moderate movement
            eat_command = 0.2f;  // Don't eat
        }
    }
    
    // Clamp values
    if (turn_direction < -1.0f) turn_direction = -1.0f;
    if (turn_direction > 1.0f) turn_direction = 1.0f;
    if (movement_strength < 0.0f) movement_strength = 0.0f;
    if (movement_strength > 1.0f) movement_strength = 1.0f;
    if (eat_command < 0.0f) eat_command = 0.0f;
    if (eat_command > 1.0f) eat_command = 1.0f;
    
    // Determine mode: eating or moving (mutually exclusive)
    if (eat_command > 0.5f) {
        fish->eating_mode = 1;
        if (fish_id == 0 && (debug_counters[fish_id] % 30) == 1) {
            printf("DEBUG: Fish %d entering eating mode\n", fish_id);
        }
    } else {
        fish->eating_mode = 0;
        
        // Apply turning: update fish heading
        float max_turn_rad = (fish_type->max_turn_angle * M_PI) / 180.0f;
        float turn_amount = turn_direction * max_turn_rad;
        float old_heading = fish->heading;
        fish->heading += turn_amount;
        
        // Normalize heading to [0, 2π]
        while (fish->heading < 0.0f) fish->heading += 2.0f * M_PI;
        while (fish->heading >= 2.0f * M_PI) fish->heading -= 2.0f * M_PI;
        
        // NEW: Apply force in heading direction (not direct velocity setting)
        Node* nodes = simulation_get_nodes();
        Node* fish_node = &nodes[fish->node_id];
        
        float force_magnitude = movement_strength * fish_type->max_force;
        float force_x = cos(fish->heading) * force_magnitude;
        float force_y = sin(fish->heading) * force_magnitude;
        
        // Add force to existing velocity (rigidbody-like behavior)
        fish_node->vx += force_x;
        fish_node->vy += force_y;
        
        // Optional: Apply speed limiting to prevent runaway acceleration
        float current_speed = sqrt(fish_node->vx * fish_node->vx + fish_node->vy * fish_node->vy);
        if (current_speed > fish_type->max_speed) {
            fish_node->vx = (fish_node->vx / current_speed) * fish_type->max_speed;
            fish_node->vy = (fish_node->vy / current_speed) * fish_type->max_speed;
        }
        
        if (fish_id < 3 && (debug_counters[fish_id] % 30) == 1) {  // Debug first 3 fish
            printf("DEBUG: Fish %d movement - turn: %.3f, heading: %.1f° -> %.1f°, force: (%.2f,%.2f), vel: (%.2f,%.2f), speed: %.2f\n",
                   fish_id, turn_direction, old_heading * 180.0f / M_PI, 
                   fish->heading * 180.0f / M_PI, force_x, force_y, fish_node->vx, fish_node->vy, current_speed);
        }
    }
}

// Calculate rewards based on RL system
void fish_calculate_rl_rewards(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    fish->last_reward = 0.0f;
    
    // 1. Oxygen reward - continuous based on current oxygen level
    float oxygen_level = gas_get_oxygen_at(fish_node->x, fish_node->y);
    float oxygen_reward = oxygen_level * fish_type->oxygen_reward_factor;
    fish->last_reward += oxygen_reward;
    
    // 2. Plant proximity reward - inverse distance to nearest plant
    float distance_to_plant = fish_get_distance_to_nearest_plant(fish_id);
    if (distance_to_plant < 1000.0f) {  // Reasonable range
        float proximity_reward = fish_type->proximity_reward_factor * (1000.0f / (distance_to_plant + 1.0f));
        fish->last_reward += proximity_reward;
    }
    
    // 3. Handle eating attempts
    if (fish->eating_mode) {
        // Fish is trying to eat - check if successful
        int ate_something = fish_attempt_eating(fish_id);
        
        if (!ate_something) {
            // Failed eating attempt - small punishment
            fish->last_reward += fish_type->eat_punishment;
        }
        // Eating success reward is handled in fish_attempt_eating()
    }
    
    // 4. Base survival reward
    fish->last_reward += 0.001f;
    
    // Track total reward
    fish->total_reward += fish->last_reward;
}

// Attempt to eat nearby plants
int fish_attempt_eating(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return 0;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    
    // Check for plants in eating range
    GridCell* cells[9];
    int cell_count = grid_get_cells_at_position(fish_x, fish_y, cells, 9);
    
    for (int c = 0; c < cell_count; c++) {
        GridCell* cell = cells[c];
        if (!cell) continue;
        
        for (int k = 0; k < cell->count; k++) {
            int node_id = cell->node_indices[k];
            if (node_id < 0 || node_id >= simulation_get_node_count()) continue;
            if (!nodes[node_id].active) continue;
            if (nodes[node_id].plant_type == -1) continue;  // Skip fish nodes
            
            float dx = nodes[node_id].x - fish_x;
            float dy = nodes[node_id].y - fish_y;
            float distance_sq = dx * dx + dy * dy;
            
            if (distance_sq <= eating_range_sq) {
                // Found plant in range - eat it!
                int plant_type = nodes[node_id].plant_type;
                float nutrition_value = calculate_plant_nutrition_value(plant_type);
                
                // Add nutrition to stomach
                fish->stomach_contents += nutrition_value;
                fish_internal_add_consumed_nutrition(nutrition_value);
                
                // Large eating reward proportional to nutrition value
                float eating_reward = nutrition_value * 20.0f;  // Scale factor
                fish->last_reward += eating_reward;
                
                // Remove eaten plant
                nodes[node_id].active = 0;
                nodes[node_id].can_grow = 0;
                
                fish->last_eating_frame = simulation_get_frame_counter();
                
                PlantType* pt = plants_get_type(plant_type);
                printf("Fish %d ate %s (nutrition: %.3f, reward: %.2f)\n", 
                       fish_id, pt ? pt->name : "plant", nutrition_value, eating_reward);
                
                return 1;  // Successfully ate
            }
        }
    }
    
    return 0;  // Nothing to eat in range
}

// Simple defecation when stomach is full
void fish_defecate(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    // Defecate when stomach is reasonably full
    if (fish->stomach_contents < 0.6f) {
        return;
    }
    
    // Simple defecation probability
    if ((float)rand() / RAND_MAX > 0.01f) {  // 1% chance per frame
        return;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float defecation_amount = fish->stomach_contents;
    
    // Add nutrition back to environment
    nutrition_add_at_position(fish_node->x, fish_node->y, 
                              defecation_amount, 80.0f);  // Fixed radius
    
    fish->stomach_contents = 0.0f;
    fish_internal_add_defecated_nutrition(defecation_amount);
    
    // Small reward for completing nutrition cycle
    fish->last_reward += defecation_amount * 2.0f;
    
    printf("Fish %d defecated %.3f nutrition\n", fish_id, defecation_amount);
    
    // Chance to spawn seed
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
                    fish->last_reward += 0.05f;  // Bonus for seed dispersal
                }
            }
        }
    }
}

// RL state accessors for Python API
float fish_get_rl_input(int fish_id, int input_index) {
    if (input_index < 0 || input_index >= RL_INPUT_SIZE) return 0.0f;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0.0f;
    
    return fish->rl_inputs[input_index];
}

void fish_set_rl_output(int fish_id, int output_index, float value) {
    if (output_index < 0 || output_index >= RL_OUTPUT_SIZE) return;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    fish->rl_outputs[output_index] = value;
    
    // DEBUG: Track when RL outputs are being set for all fish
    static int set_counters[MAX_FISH] = {0};
    if (fish_id < MAX_FISH && (set_counters[fish_id]++ % 30) == 0) {
        printf("DEBUG fish_set_rl_output: Fish %d output[%d] = %.3f (every frame)\n", fish_id, output_index, value);
    }
}

float fish_get_last_reward(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->last_reward : 0.0f;
}

// Legacy compatibility functions
void fish_set_movement_force(int fish_id, float force_x, float force_y) {
    // No longer used in RL system
    (void)fish_id;
    (void)force_x;
    (void)force_y;
}

void fish_clear_movement_force(int fish_id) {
    // No longer used in RL system
    (void)fish_id;
}

void fish_apply_rl_action(int fish_id, float direction_x, float direction_y) {
    // Legacy function - no longer used
    (void)fish_id;
    (void)direction_x;
    (void)direction_y;
}

void fish_update_oxygen_system(int fish_id) {
    // No longer needed - oxygen is direct input to RL
    (void)fish_id;
}

void fish_update_hunger_system(int fish_id) {
    // No longer needed - simplified to stomach contents only
    (void)fish_id;
}

void fish_update_rl_state(int fish_id) {
    // Replaced by fish_update_rl_inputs
    fish_update_rl_inputs(fish_id);
}

void fish_calculate_environmental_rewards(int fish_id) {
    // Replaced by fish_calculate_rl_rewards
    (void)fish_id;
}

void fish_calculate_chemoreceptor_rewards(int fish_id) {
    // Replaced by fish_calculate_rl_rewards  
    (void)fish_id;
}

void fish_eat_nearby_plants(int fish_id) {
    // Eating is now handled by RL outputs
    (void)fish_id;
}

int fish_can_eat_plant(int fish_id, int node_id) {
    // Legacy function
    (void)fish_id;
    (void)node_id;
    return 0;
}

float fish_get_oxygen_level(int fish_id) {
    // Return current environmental oxygen instead
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0.0f;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    return gas_get_oxygen_at(fish_node->x, fish_node->y);
}

float fish_get_hunger_level(int fish_id) {
    // Return inverse of stomach contents as hunger
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 1.0f;
    
    return fmaxf(0.0f, 1.0f - fish->stomach_contents);
}

float fish_get_saturation_level(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->stomach_contents : 0.0f;
}

float fish_get_reward(int fish_id) {
    return fish_get_last_reward(fish_id);
}