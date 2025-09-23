// fish_behaviour.c - Updated to properly track environmental nutrition balance
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

// Track parent fish for neural network inheritance
static int g_parent_fish_id = -1;
static int g_reproduction_notification_pending = 0;

// Track previous positions for movement analysis
static float g_previous_positions_x[MAX_FISH] = {0};
static float g_previous_positions_y[MAX_FISH] = {0};
static int g_position_tracking_initialized = 0;

// Calculate corpse nutrition value based on original fish type
static float calculate_corpse_nutrition_value(int original_fish_type) {
    if (original_fish_type < 0 || original_fish_type >= fish_get_type_count()) {
        return 0.15f;
    }
    
    FishType* ft = fish_get_type(original_fish_type);
    if (!ft) return 0.15f;
    
    float nutrition_value = ft->size_radius * 0.02f + ft->danger_level * 0.1f + 0.1f;
    
    return nutrition_value;
}

// Apply RL outputs with enhanced movement mechanics
void fish_apply_rl_outputs(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;

    // Update eating cooldown
    if (fish->eating_cooldown > 0) {
        fish->eating_cooldown--;
    }

    // Parse RL outputs
    float turn_direction = fish->rl_outputs[0];
    float movement_strength = fish->rl_outputs[1];
    float eat_command = fish->rl_outputs[2];

    // Clamp values
    if (turn_direction < -1.0f) turn_direction = -1.0f;
    if (turn_direction > 1.0f) turn_direction = 1.0f;
    if (movement_strength < 0.0f) movement_strength = 0.0f;
    if (movement_strength > 1.0f) movement_strength = 1.0f;
    if (eat_command < 0.0f) eat_command = 0.0f;
    if (eat_command > 1.0f) eat_command = 1.0f;

    // Determine mode: eating or moving
    if (eat_command > 0.5f) {
        fish->eating_mode = 1;
    } else {
        fish->eating_mode = 0;
        
        // Apply turning
        float max_turn_rad = (fish_type->max_turn_angle * M_PI) / 180.0f;
        float turn_amount = turn_direction * max_turn_rad;
        fish->heading += turn_amount;

        // Normalize heading
        while (fish->heading < 0.0f) fish->heading += 2.0f * M_PI;
        while (fish->heading >= 2.0f * M_PI) fish->heading -= 2.0f * M_PI;

        // Apply force in heading direction
        Node* nodes = simulation_get_nodes();
        Node* fish_node = &nodes[fish->node_id];
        
        float force_magnitude = movement_strength * fish_type->max_force;
        
        // Enhanced movement for better learning
        if (movement_strength > 0.7f) {
            force_magnitude *= 1.2f;
        }
        
        float force_x = cos(fish->heading) * force_magnitude;
        float force_y = sin(fish->heading) * force_magnitude;

        fish_node->vx += force_x;
        fish_node->vy += force_y;

        // Speed limiting
        float current_speed = sqrt(fish_node->vx * fish_node->vx + fish_node->vy * fish_node->vy);
        if (current_speed > fish_type->max_speed) {
            fish_node->vx = (fish_node->vx / current_speed) * fish_type->max_speed;
            fish_node->vy = (fish_node->vy / current_speed) * fish_type->max_speed;
        }
    }
}

// Enhanced reward calculation with predator approach rewards
void fish_calculate_rl_rewards(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    fish->last_reward = 0.0f;
    
    // Initialize position tracking on first call
    if (!g_position_tracking_initialized) {
        for (int i = 0; i < MAX_FISH; i++) {
            g_previous_positions_x[i] = 0.0f;
            g_previous_positions_y[i] = 0.0f;
        }
        g_position_tracking_initialized = 1;
    }
    
    // Basic survival reward
    fish->last_reward += 0.001f;
    
    // Anti-spinning penalty
    float turn_amount = fabs(fish->rl_outputs[0]);
    if (turn_amount > 0.6f) {
        float spin_penalty = -0.08f * (turn_amount - 0.6f) / 0.4f;
        fish->last_reward += spin_penalty;
    }
    
    // Track consecutive high-turn frames to detect spinning
    static int high_turn_counters[MAX_FISH] = {0};
    static float last_turn_directions[MAX_FISH] = {0};
    
    if (turn_amount > 0.7f) {
        high_turn_counters[fish_id]++;
        
        if (high_turn_counters[fish_id] > 3) {
            float direction_consistency = fish->rl_outputs[0] * last_turn_directions[fish_id];
            if (direction_consistency > 0) {
                float spin_duration_penalty = -0.05f * fminf((high_turn_counters[fish_id] - 5) / 10.0f, 1.0f);
                fish->last_reward += spin_duration_penalty;
            }
        }
    } else {
        high_turn_counters[fish_id] = 0;
    }
    
    last_turn_directions[fish_id] = fish->rl_outputs[0];
    
    // Species-specific rewards
    if (fish_type->is_predator) {
        // ENHANCED PREDATOR REWARDS: Approach and distance-based rewards
        
        // Get target info from inputs 0,1,3 (prey vector and distance)
        float prey_vector_x = fish->rl_inputs[0];
        float prey_vector_y = fish->rl_inputs[1];
        float prey_distance = fish->rl_inputs[3];  // Normalized distance (0=close, 1=far)
        
        float prey_magnitude = sqrt(prey_vector_x * prey_vector_x + prey_vector_y * prey_vector_y);
        
        if (prey_magnitude > 0.1f) {  // Prey detected
            // MASSIVE proximity reward - closer is much better
            float proximity_factor = 1.0f - prey_distance;  // 1=very close, 0=far
            float proximity_reward = 0.5f * proximity_factor * proximity_factor;  // Quadratic reward
            fish->last_reward += proximity_reward;
            
            // MOVEMENT TOWARDS PREY REWARD
            float current_x = fish_node->x;
            float current_y = fish_node->y;
            float prev_x = g_previous_positions_x[fish_id];
            float prev_y = g_previous_positions_y[fish_id];
            
            // Calculate if fish moved closer to prey
            if (prev_x != 0.0f || prev_y != 0.0f) {  // Skip first frame
                float dx_prev = current_x - prev_x;
                float dy_prev = current_y - prev_y;
                float movement_distance = sqrt(dx_prev * dx_prev + dy_prev * dy_prev);
                
                if (movement_distance > 0.5f) {  // Only if fish actually moved
                    // Check if movement was towards prey
                    float movement_x = dx_prev / movement_distance;
                    float movement_y = dy_prev / movement_distance;
                    
                    // Dot product: positive = moving towards prey
                    float approach_alignment = prey_vector_x * movement_x + prey_vector_y * movement_y;
                    
                    if (approach_alignment > 0.3f) {  // Moving towards prey
                        float approach_reward = 0.3f * approach_alignment * proximity_factor;
                        fish->last_reward += approach_reward;
                    } else if (approach_alignment < -0.3f) {  // Moving away from prey
                        float retreat_penalty = -0.12f * (-approach_alignment) * proximity_factor;
                        fish->last_reward += retreat_penalty;
                    }
                }
            }
        }
        
        // Threat avoidance (from inputs 4,5,6 - stronger predators)
        float threat_magnitude = sqrt(fish->rl_inputs[4] * fish->rl_inputs[4] + 
                                      fish->rl_inputs[5] * fish->rl_inputs[5]);
        float danger_level = fish->rl_inputs[6];
        
        if (threat_magnitude > 0.1f && danger_level < -0.1f) {
            float threat_level = -danger_level;
            
            // Movement analysis for threat avoidance
            float current_x = fish_node->x;
            float current_y = fish_node->y;
            float prev_x = g_previous_positions_x[fish_id];
            float prev_y = g_previous_positions_y[fish_id];
            
            if (prev_x != 0.0f || prev_y != 0.0f) {
                float dx_prev = current_x - prev_x;
                float dy_prev = current_y - prev_y;
                float movement_distance = sqrt(dx_prev * dx_prev + dy_prev * dy_prev);
                
                if (movement_distance > 0.5f) {
                    float movement_x = dx_prev / movement_distance;
                    float movement_y = dy_prev / movement_distance;
                    
                    // Check if moving away from threat
                    float escape_alignment = -(fish->rl_inputs[4] * movement_x + fish->rl_inputs[5] * movement_y);
                    
                    if (escape_alignment > 0.3f) {
                        float escape_reward = 0.08f * threat_level * escape_alignment;
                        fish->last_reward += escape_reward;
                    } else if (escape_alignment < -0.3f) {
                        float approach_threat_penalty = -0.15f * threat_level * (-escape_alignment);
                        fish->last_reward += approach_threat_penalty;
                    }
                }
            }
        }
        
    } else {
        // HERBIVORE REWARDS
        
        // Plant proximity rewards (inputs 0,1,3)
        float plant_distance = fish->rl_inputs[3];  // Normalized distance
        if (plant_distance < 1.0f) {  // Plant detected
            float proximity_factor = 1.0f - plant_distance;
            // 10x multiplier for plant proximity (already very high)
            fish->last_reward += fish_type->proximity_reward_factor * proximity_factor * 20.0f;
        }
        
        // Additional close-range plant bonus
        float estimated_plant_distance = plant_distance * 500.0f;  // Convert back to world units
        if (estimated_plant_distance < 100.0f) {
            float close_proximity = 1.0f - (estimated_plant_distance / 100.0f);
            fish->last_reward += 0.08f * close_proximity;
        }
        
        // Oxygen level reward
        float oxygen_level = gas_get_oxygen_at(fish_node->x, fish_node->y);
        fish->last_reward += oxygen_level * fish_type->oxygen_reward_factor;
        
        // Enhanced predator avoidance rewards (inputs 4,5,6)
        float danger_level = fish->rl_inputs[6];
        float foreign_fish_magnitude = sqrt(fish->rl_inputs[4] * fish->rl_inputs[4] + 
                                            fish->rl_inputs[5] * fish->rl_inputs[5]);
        
        if (foreign_fish_magnitude > 0.1f && danger_level < -0.1f) {
            float threat_level = -danger_level;
            
            // Movement analysis for escape behavior
            float current_x = fish_node->x;
            float current_y = fish_node->y;
            float prev_x = g_previous_positions_x[fish_id];
            float prev_y = g_previous_positions_y[fish_id];
            
            if (prev_x != 0.0f || prev_y != 0.0f) {
                float dx_prev = current_x - prev_x;
                float dy_prev = current_y - prev_y;
                float current_speed = sqrt(dx_prev * dx_prev + dy_prev * dy_prev);
                
                if (current_speed > 0.5f) {
                    float move_dir_x = dx_prev / current_speed;
                    float move_dir_y = dy_prev / current_speed;
                    
                    // Check if moving away from threat
                    float alignment = -(fish->rl_inputs[4] * move_dir_x + fish->rl_inputs[5] * move_dir_y);
                    
                    if (alignment > 0.3f) {  // Moving away from threat
                        float escape_reward = 0.25f * threat_level * alignment * (current_speed / fish_type->max_speed);
                        fish->last_reward += escape_reward;
                    } else if (alignment < -0.3f) {  // Moving toward threat
                        float approach_punishment = -0.20f * threat_level * (-alignment);
                        fish->last_reward += approach_punishment;
                    }
                }
            }
            
            // Additional reward for detecting threats
            fish->last_reward += 0.02f * threat_level;
        }
    }
    
    // Store current position for next frame analysis
    g_previous_positions_x[fish_id] = fish_node->x;
    g_previous_positions_y[fish_id] = fish_node->y;
    
    // World boundary punishment
    if (fish_node->x <= WORLD_LEFT + 50 || fish_node->x >= WORLD_RIGHT - 50 ||
        fish_node->y <= WORLD_TOP + 50 || fish_node->y >= WORLD_BOTTOM - 50) {
        fish->last_reward -= 0.02f;
    }
    
    // Eating behavior
    if (fish->eating_mode) {
        int ate_something = 0;
        
        if (fish_type->is_predator) {
            ate_something = fish_attempt_eating_fish(fish_id);
            if (!ate_something) {
                ate_something = fish_attempt_eating_corpse(fish_id);
            }
        } else {
            ate_something = fish_attempt_eating_plant(fish_id);
        }
        
        if (!ate_something) {
            fish->last_reward += fish_type->eat_punishment;
        }
    }
    
    fish->total_reward += fish->last_reward;
}

// Plant eating with proper nutrition tracking
int fish_attempt_eating_plant(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || fish_type->is_predator) return 0;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    
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
            if (nodes[node_id].is_corpse) continue;         // Skip corpses
            
            // Skip seeds with immunity
            if (nodes[node_id].seed_immunity_timer > 0) {
                continue;
            }
            
            float dx = nodes[node_id].x - fish_x;
            float dy = nodes[node_id].y - fish_y;
            float distance_sq = dx * dx + dy * dy;
            
            if (distance_sq <= eating_range_sq) {
                // Get stored nutrition from the plant
                float nutrition_value = nodes[node_id].stored_nutrition;
                
                // Add to fish stomach
                fish->stomach_contents += nutrition_value;
                fish_internal_add_consumed_nutrition(nutrition_value);
                
                // Large eating reward for NN learning
                float eating_reward = nutrition_value * 40.0f;
                fish->last_reward += eating_reward;
                
                // Remove plant from simulation
                nodes[node_id].active = 0;
                nodes[node_id].can_grow = 0;
                
                fish->last_eating_frame = simulation_get_frame_counter();
                
                return 1;
            }
        }
    }
    
    return 0;
}

// Attempt to eat fish (predators only)
int fish_attempt_eating_fish(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || !fish_type->is_predator) return 0;
    
    if (fish->eating_cooldown > 0) {
        fish->last_reward += 0.002f;
        return 0;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    Fish* all_fish = fish_get_all();
    int fish_count = fish_get_count();
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    
    for (int i = 0; i < fish_count; i++) {
        if (!all_fish[i].active) continue;
        if (i == fish_id) continue;
        if (all_fish[i].fish_type == fish->fish_type) continue;
        
        FishType* prey_type = fish_get_type(all_fish[i].fish_type);
        if (!prey_type) continue;
        
        if (prey_type->danger_level >= fish_type->danger_level) continue;
        
        Node* prey_node = &nodes[all_fish[i].node_id];
        if (!prey_node->active) continue;
        
        float dx = prey_node->x - fish_x;
        float dy = prey_node->y - fish_y;
        float distance_sq = dx * dx + dy * dy;
        
        if (distance_sq <= eating_range_sq) {
            // Successfully caught prey
            float predation_reward = 60.0f + (fish_type->danger_level * 30.0f);
            fish->last_reward += predation_reward;
            
            // Remove prey fish
            all_fish[i].active = 0;
            prey_node->active = 0;
            
            fish->eating_cooldown = fish_type->eating_cooldown_frames;
            
            return 1;
        }
    }
    
    return 0;
}

// Attempt to eat corpse (predators only)
int fish_attempt_eating_corpse(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || !fish_type->is_predator) return 0;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    
    GridCell* cells[9];
    int cell_count = grid_get_cells_at_position(fish_x, fish_y, cells, 9);
    
    for (int c = 0; c < cell_count; c++) {
        GridCell* cell = cells[c];
        if (!cell) continue;
        
        for (int k = 0; k < cell->count; k++) {
            int node_id = cell->node_indices[k];
            if (node_id < 0 || node_id >= simulation_get_node_count()) continue;
            if (!nodes[node_id].active) continue;
            if (!nodes[node_id].is_corpse) continue;  // Only corpses
            
            float dx = nodes[node_id].x - fish_x;
            float dy = nodes[node_id].y - fish_y;
            float distance_sq = dx * dx + dy * dy;
            
            if (distance_sq <= eating_range_sq) {
                int original_fish_type = nodes[node_id].original_fish_type;
                float nutrition_value = calculate_corpse_nutrition_value(original_fish_type);
                
                fish->stomach_contents += nutrition_value;
                fish_internal_add_consumed_nutrition(nutrition_value);
                
                // Good reward for eating corpse
                float eating_reward = nutrition_value * 35.0f;
                fish->last_reward += eating_reward;
                
                // Remove corpse
                nodes[node_id].active = 0;
                
                fish->last_eating_frame = simulation_get_frame_counter();
                fish_increment_corpses_eaten();
                
                return 1;
            }
        }
    }
    
    return 0;
}

// Defecation with environmental nutrition tracking
void fish_defecate(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    // Only herbivores defecate
    if (fish_type->is_predator) return;
    
    // Check if stomach is 70% full
    if (fish->stomach_contents < 0.7f) {
        return;
    }
    
    // 1% chance per frame to defecate when stomach is full enough
    if ((float)rand() / RAND_MAX > 0.01f) {
        return;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Complete stomach emptying
    float full_stomach_contents = fish->stomach_contents;
    
    // ADD nutrition back to environment layer
    nutrition_add_at_position(fish_node->x, fish_node->y, full_stomach_contents, 
                              STANDARD_DEPLETION_RANGE);
    
    // ADD to environmental nutrition balance (nutrition returned to ground)
    plants_add_environmental_nutrition(full_stomach_contents);
    
    // Empty stomach completely
    fish->stomach_contents = 0.0f;
    fish_internal_add_defecated_nutrition(full_stomach_contents);
    
    fish->defecation_count++;
    
    fish->last_reward += full_stomach_contents * 3.0f;
    
    // Reproduction after 3 defecations (herbivores only)
    if (fish->defecation_count >= 3) {
        g_parent_fish_id = fish_id;
        fish_reproduce(fish_id);
        fish->defecation_count = 0;
    }
    
    // Plant seeding with immunity
    if ((float)rand() / RAND_MAX < 0.25f) {
        int plant_type_count = plants_get_type_count();
        if (plant_type_count > 0) {
            int random_plant_type = rand() % plant_type_count;
            
            // Enhanced seed dispersal - away from fish
            float dispersal_angle = fish->heading + M_PI + ((float)rand() / RAND_MAX - 0.5f) * 1.5f;
            float dispersal_distance = 60.0f + ((float)rand() / RAND_MAX) * 40.0f;
            
            float seed_x = fish_node->x + cos(dispersal_angle) * dispersal_distance;
            float seed_y = fish_node->y + sin(dispersal_angle) * dispersal_distance;
            
            if (seed_x >= WORLD_LEFT && seed_x <= WORLD_RIGHT &&
                seed_y >= WORLD_TOP && seed_y <= WORLD_BOTTOM) {
                
                // Use new immune seed creation function
                int new_node = simulation_add_seed_node(seed_x, seed_y, random_plant_type);
                if (new_node >= 0) {
                    fish->last_reward += 0.05f;
                }
            }
        }
    }
}

// Enhanced reproduction system with parent tracking
void fish_reproduce(int fish_id) {
    Fish* parent_fish = fish_get_by_id(fish_id);
    if (!parent_fish) return;
    
    FishType* fish_type = fish_get_type(parent_fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* parent_node = &nodes[parent_fish->node_id];
    
    // Spawn offspring near parent
    float spawn_distance = 120.0f;
    float spawn_angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
    float spawn_x = parent_node->x + cos(spawn_angle) * spawn_distance;
    float spawn_y = parent_node->y + sin(spawn_angle) * spawn_distance;
    
    // Clamp to world bounds
    if (spawn_x < WORLD_LEFT + 20) spawn_x = WORLD_LEFT + 20;
    if (spawn_x > WORLD_RIGHT - 20) spawn_x = WORLD_RIGHT - 20;
    if (spawn_y < WORLD_TOP + 20) spawn_y = WORLD_TOP + 20;
    if (spawn_y > WORLD_BOTTOM - 20) spawn_y = WORLD_BOTTOM - 20;
    
    int offspring_id = fish_add(spawn_x, spawn_y, parent_fish->fish_type);
    
    if (offspring_id >= 0) {
        // Huge reproduction reward
        parent_fish->last_reward += 150.0f;
        
        // Set flag for Python to know about inheritance
        g_reproduction_notification_pending = 1;
    }
    
    g_parent_fish_id = -1;
}

// Predator reproduction (separate system, based on kills)
void fish_predator_reproduce(int fish_id) {
    Fish* predator = fish_get_by_id(fish_id);
    if (!predator) return;
    
    FishType* fish_type = fish_get_type(predator->fish_type);
    if (!fish_type || !fish_type->is_predator) return;
    
    // Track kills using a static counter
    static int kill_counts[MAX_FISH] = {0};
    
    // Check if predator just made a kill (eating cooldown was just set)
    if (predator->eating_cooldown == fish_type->eating_cooldown_frames) {
        // Just made a kill
        kill_counts[fish_id]++;
        
        if (kill_counts[fish_id] >= 3) {
            kill_counts[fish_id] = 0;
            
            Node* nodes = simulation_get_nodes();
            Node* parent_node = &nodes[predator->node_id];
            
            float spawn_distance = 150.0f;
            float spawn_angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
            float spawn_x = parent_node->x + cos(spawn_angle) * spawn_distance;
            float spawn_y = parent_node->y + sin(spawn_angle) * spawn_distance;
            
            // Clamp to world bounds
            if (spawn_x < WORLD_LEFT + 20) spawn_x = WORLD_LEFT + 20;
            if (spawn_x > WORLD_RIGHT - 20) spawn_x = WORLD_RIGHT - 20;
            if (spawn_y < WORLD_TOP + 20) spawn_y = WORLD_TOP + 20;
            if (spawn_y > WORLD_BOTTOM - 20) spawn_y = WORLD_BOTTOM - 20;
            
            g_parent_fish_id = fish_id;
            int offspring_id = fish_add(spawn_x, spawn_y, predator->fish_type);
            
            if (offspring_id >= 0) {
                predator->last_reward += 200.0f;
            }
            
            g_parent_fish_id = -1;
        }
    }
}

// Get parent fish ID for inheritance (called by Python)
int fish_get_parent_for_inheritance(void) {
    return g_parent_fish_id;
}

// Check if reproduction notification is pending
int fish_is_reproduction_pending(void) {
    if (g_reproduction_notification_pending) {
        g_reproduction_notification_pending = 0;
        return 1;
    }
    return 0;
}

// Wrapper function for backward compatibility
int fish_attempt_eating(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return 0;
    
    if (fish_type->is_predator) {
        // Try fish first, then corpses
        int ate_fish = fish_attempt_eating_fish(fish_id);
        if (ate_fish) return 1;
        
        return fish_attempt_eating_corpse(fish_id);
    } else {
        return fish_attempt_eating_plant(fish_id);
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
}

float fish_get_last_reward(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->last_reward : 0.0f;
}

// Legacy compatibility functions
void fish_set_movement_force(int fish_id, float force_x, float force_y) {
    (void)fish_id; (void)force_x; (void)force_y;
}

void fish_clear_movement_force(int fish_id) {
    (void)fish_id;
}

void fish_apply_rl_action(int fish_id, float direction_x, float direction_y) {
    (void)fish_id; (void)direction_x; (void)direction_y;
}

void fish_update_oxygen_system(int fish_id) {
    (void)fish_id;
}

void fish_update_hunger_system(int fish_id) {
    (void)fish_id;
}

void fish_update_rl_state(int fish_id) {
    fish_update_rl_inputs(fish_id);
}

void fish_calculate_environmental_rewards(int fish_id) {
    (void)fish_id;
}

void fish_calculate_chemoreceptor_rewards(int fish_id) {
    (void)fish_id;
}

void fish_eat_nearby_plants(int fish_id) {
    (void)fish_id;
}

int fish_can_eat_plant(int fish_id, int node_id) {
    (void)fish_id; (void)node_id;
    return 0;
}

float fish_get_oxygen_level(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0.0f;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    return gas_get_oxygen_at(fish_node->x, fish_node->y);
}

float fish_get_hunger_level(int fish_id) {
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