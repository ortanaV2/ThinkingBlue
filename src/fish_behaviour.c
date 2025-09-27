// fish_behaviour.c - Neural network behavior system for marine life simulation
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

// Neural network inheritance tracking for reproduction
static int g_parent_fish_id = -1;
static int g_reproduction_notification_pending = 0;

// Movement tracking for dynamic reward calculation
static float g_previous_positions_x[MAX_FISH] = {0};
static float g_previous_positions_y[MAX_FISH] = {0};
static int g_position_tracking_initialized = 0;

// Calculate nutrition value from corpse based on original fish type
static float calculate_corpse_nutrition_value(int original_fish_type) {
    if (original_fish_type < 0 || original_fish_type >= fish_get_type_count()) {
        return 0.15f;
    }
    
    FishType* ft = fish_get_type(original_fish_type);
    if (!ft) return 0.15f;
    
    // Larger, more dangerous fish provide more nutrition
    float nutrition_value = ft->size_radius * 0.02f + ft->danger_level * 0.1f + 0.1f;
    
    return nutrition_value;
}

// Apply neural network outputs to control fish movement and behavior
void fish_apply_rl_outputs(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;

    // Update eating cooldown timer
    if (fish->eating_cooldown > 0) {
        fish->eating_cooldown--;
    }

    // Extract neural network outputs
    float turn_direction = fish->rl_outputs[0];     // -1.0 to 1.0 (left to right)
    float movement_strength = fish->rl_outputs[1];  // 0.0 to 1.0 (stopped to full speed)
    float eat_command = fish->rl_outputs[2];        // 0.0 to 1.0 (ignore to eat)

    // Clamp outputs to valid ranges
    turn_direction = fmaxf(-1.0f, fminf(1.0f, turn_direction));
    movement_strength = fmaxf(0.0f, fminf(1.0f, movement_strength));
    eat_command = fmaxf(0.0f, fminf(1.0f, eat_command));

    // Determine behavior mode
    if (eat_command > 0.5f) {
        fish->eating_mode = 1;  // Try to eat
    } else {
        fish->eating_mode = 0;  // Normal movement
        
        // Apply turning behavior
        float max_turn_rad = (fish_type->max_turn_angle * M_PI) / 180.0f;
        float turn_amount = turn_direction * max_turn_rad;
        fish->heading += turn_amount;

        // Keep heading in valid range [0, 2π]
        while (fish->heading < 0.0f) fish->heading += 2.0f * M_PI;
        while (fish->heading >= 2.0f * M_PI) fish->heading -= 2.0f * M_PI;

        // Apply forward movement force
        Node* nodes = simulation_get_nodes();
        Node* fish_node = &nodes[fish->node_id];
        
        float force_magnitude = movement_strength * fish_type->max_force;
        
        // Boost force for high movement commands (improves learning)
        if (movement_strength > 0.7f) {
            force_magnitude *= 1.2f;
        }
        
        // Calculate force components
        float force_x = cos(fish->heading) * force_magnitude;
        float force_y = sin(fish->heading) * force_magnitude;

        // Apply forces to fish velocity
        fish_node->vx += force_x;
        fish_node->vy += force_y;

        // Enforce maximum speed limit
        float current_speed = sqrt(fish_node->vx * fish_node->vx + fish_node->vy * fish_node->vy);
        if (current_speed > fish_type->max_speed) {
            fish_node->vx = (fish_node->vx / current_speed) * fish_type->max_speed;
            fish_node->vy = (fish_node->vy / current_speed) * fish_type->max_speed;
        }
    }
}

// Calculate neural network rewards based on fish behavior and environment
void fish_calculate_rl_rewards(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    fish->last_reward = 0.0f;
    
    // Initialize position tracking system
    if (!g_position_tracking_initialized) {
        for (int i = 0; i < MAX_FISH; i++) {
            g_previous_positions_x[i] = 0.0f;
            g_previous_positions_y[i] = 0.0f;
        }
        g_position_tracking_initialized = 1;
    }
    
    // Base survival reward
    fish->last_reward += 0.001f;
    
    // Oxygen level reward (important for all fish)
    float oxygen_level = gas_get_oxygen_at(fish_node->x, fish_node->y);
    fish->last_reward += oxygen_level * fish_type->oxygen_reward_factor;
    
    // Anti-spinning penalty to discourage excessive turning
    float turn_amount = fabs(fish->rl_outputs[0]);
    if (turn_amount > 0.6f) {
        float spin_penalty = -0.08f * (turn_amount - 0.6f) / 0.4f;
        fish->last_reward += spin_penalty;
    }
    
    // Track and penalize persistent spinning behavior
    static int high_turn_counters[MAX_FISH] = {0};
    static float last_turn_directions[MAX_FISH] = {0};
    
    if (turn_amount > 0.7f) {
        high_turn_counters[fish_id]++;
        
        // Penalize sustained spinning in same direction
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
    
    // Species-specific reward systems
    if (fish_type->is_predator) {
        // PREDATOR REWARD SYSTEM: Focus on hunting behavior
        
        // Extract prey information from neural network inputs
        float prey_vector_x = fish->rl_inputs[0];   // Direction to nearest prey
        float prey_vector_y = fish->rl_inputs[1];
        float prey_distance = fish->rl_inputs[3];   // Normalized distance (0=close, 1=far)
        
        float prey_magnitude = sqrt(prey_vector_x * prey_vector_x + prey_vector_y * prey_vector_y);
        
        if (prey_magnitude > 0.1f) {  // Prey detected
            // Reward proximity to prey (quadratic to emphasize close encounters)
            float proximity_factor = 1.0f - prey_distance;
            float proximity_reward = 0.4f * proximity_factor * proximity_factor;
            fish->last_reward += proximity_reward;
            
            // Reward for orienting toward prey
            float target_angle = atan2(prey_vector_y, prey_vector_x);
            float heading_alignment = cos(fish->heading - target_angle);
            if (heading_alignment > 0.3f) {
                float hunting_reward = 0.1f * heading_alignment * proximity_factor;
                fish->last_reward += hunting_reward;
            }
            
            // Dynamic movement reward - encourage active pursuit
            float current_x = fish_node->x;
            float current_y = fish_node->y;
            float prev_x = g_previous_positions_x[fish_id];
            float prev_y = g_previous_positions_y[fish_id];
            
            if (prev_x != 0.0f || prev_y != 0.0f) {  // Skip first frame
                float dx_prev = current_x - prev_x;
                float dy_prev = current_y - prev_y;
                float movement_distance = sqrt(dx_prev * dx_prev + dy_prev * dy_prev);
                
                if (movement_distance > 0.5f) {
                    // Calculate movement direction
                    float movement_x = dx_prev / movement_distance;
                    float movement_y = dy_prev / movement_distance;
                    
                    // Reward movement toward prey
                    float approach_alignment = prey_vector_x * movement_x + prey_vector_y * movement_y;
                    
                    if (approach_alignment > 0.1f) {
                        float speed_factor = movement_distance / fish_type->max_speed;
                        float dynamic_approach_reward = 1.5f * approach_alignment * proximity_factor * (1.0f + speed_factor);
                        fish->last_reward += dynamic_approach_reward;
                    }
                } else {
                    // Penalize staying still when prey is visible
                    float stillness_penalty = -0.05f * proximity_factor;
                    fish->last_reward += stillness_penalty;
                }
            }
        } else {
            // Reward exploration when no prey is visible
            if (fish->rl_outputs[1] > 0.3f) {
                float exploration_reward = fish->rl_outputs[1] * 0.08f;
                fish->last_reward += exploration_reward;
            }
        }
        
        // Threat avoidance for smaller predators
        float threat_magnitude = sqrt(fish->rl_inputs[4] * fish->rl_inputs[4] + 
                                      fish->rl_inputs[5] * fish->rl_inputs[5]);
        float danger_level = fish->rl_inputs[6];
        
        if (threat_magnitude > 0.1f && danger_level < -0.1f) {
            float threat_level = -danger_level;
            
            // Analyze movement relative to threats
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
                    
                    // Reward escaping from threats
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
        // HERBIVORE REWARD SYSTEM: Focus on feeding and survival
        
        // Plant proximity rewards
        float plant_distance = fish->rl_inputs[3];
        if (plant_distance < 1.0f) {
            float proximity_factor = 1.0f - plant_distance;
            // Strong reward for finding plants
            fish->last_reward += fish_type->proximity_reward_factor * proximity_factor * 20.0f;
        }
        
        // Bonus for very close plants
        float estimated_plant_distance = plant_distance * 500.0f;
        if (estimated_plant_distance < 100.0f) {
            float close_proximity = 1.0f - (estimated_plant_distance / 100.0f);
            fish->last_reward += 0.08f * close_proximity;
        }
        
        // Predator avoidance system
        float danger_level = fish->rl_inputs[6];
        float foreign_fish_magnitude = sqrt(fish->rl_inputs[4] * fish->rl_inputs[4] + 
                                            fish->rl_inputs[5] * fish->rl_inputs[5]);
        
        if (foreign_fish_magnitude > 0.1f && danger_level < -0.1f) {
            float threat_level = -danger_level;
            
            // Analyze escape behavior
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
                    
                    // Reward fleeing from predators
                    float alignment = -(fish->rl_inputs[4] * move_dir_x + fish->rl_inputs[5] * move_dir_y);
                    
                    if (alignment > 0.3f) {
                        float escape_reward = 0.25f * threat_level * alignment * (current_speed / fish_type->max_speed);
                        fish->last_reward += escape_reward;
                    } else if (alignment < -0.3f) {
                        float approach_punishment = -0.20f * threat_level * (-alignment);
                        fish->last_reward += approach_punishment;
                    }
                }
            }
            
            // Small reward for detecting threats
            fish->last_reward += 0.02f * threat_level;
        }
    }
    
    // Store position for next frame analysis
    g_previous_positions_x[fish_id] = fish_node->x;
    g_previous_positions_y[fish_id] = fish_node->y;
    
    // Boundary avoidance penalty
    if (fish_node->x <= WORLD_LEFT + 50 || fish_node->x >= WORLD_RIGHT - 50 ||
        fish_node->y <= WORLD_TOP + 50 || fish_node->y >= WORLD_BOTTOM - 50) {
        fish->last_reward -= 0.02f;
    }
    
    // Handle eating behavior
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
        
        // Penalty for unsuccessful eating attempts
        if (!ate_something) {
            fish->last_reward += fish_type->eat_punishment;
        }
    }
    
    fish->total_reward += fish->last_reward;
}

// Plant eating for herbivores with field-of-view restrictions
int fish_attempt_eating_plant(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || fish_type->is_predator) return 0;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float fish_heading = fish->heading;
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    
    // Fish can only eat plants in front of them (90° cone)
    const float EATING_FOV_ANGLE = 90.0f * M_PI / 180.0f;
    float half_eating_fov = EATING_FOV_ANGLE * 0.5f;
    
    // Use spatial grid for efficient plant searching
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
            
            // Skip immune seeds (recently planted)
            if (nodes[node_id].seed_immunity_timer > 0) {
                continue;
            }
            
            float dx = nodes[node_id].x - fish_x;
            float dy = nodes[node_id].y - fish_y;
            float distance_sq = dx * dx + dy * dy;
            
            // Check distance first (optimization)
            if (distance_sq > eating_range_sq) continue;
            
            // Check if plant is within eating field-of-view
            float angle_to_plant = atan2(dy, dx);
            float relative_angle = angle_to_plant - fish_heading;
            
            // Normalize angle to [-π, π]
            while (relative_angle > M_PI) relative_angle -= 2.0f * M_PI;
            while (relative_angle < -M_PI) relative_angle += 2.0f * M_PI;
            
            if (fabs(relative_angle) > half_eating_fov) {
                continue;  // Plant not in front of fish
            }
            
            // Successfully eat the plant
            float nutrition_value = nodes[node_id].stored_nutrition;
            
            fish->stomach_contents += nutrition_value;
            fish_internal_add_consumed_nutrition(nutrition_value);
            
            // Large reward for successful eating
            float eating_reward = nutrition_value * 40.0f;
            fish->last_reward += eating_reward;
            
            // Remove plant from simulation
            nodes[node_id].active = 0;
            nodes[node_id].can_grow = 0;
            
            fish->last_eating_frame = simulation_get_frame_counter();
            
            return 1;
        }
    }
    
    return 0;
}

// Fish hunting for predators
int fish_attempt_eating_fish(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || !fish_type->is_predator) return 0;
    
    // Check eating cooldown (prevents rapid killing)
    if (fish->eating_cooldown > 0) {
        fish->last_reward += 0.002f;  // Small patience reward
        return 0;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    Fish* all_fish = fish_get_all();
    int fish_count = fish_get_count();
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    
    // Search for prey fish within range
    for (int i = 0; i < fish_count; i++) {
        if (!all_fish[i].active) continue;
        if (i == fish_id) continue;  // Don't eat yourself
        if (all_fish[i].fish_type == fish->fish_type) continue;  // Don't eat same species
        
        FishType* prey_type = fish_get_type(all_fish[i].fish_type);
        if (!prey_type) continue;
        
        // Only hunt weaker prey
        if (prey_type->danger_level >= fish_type->danger_level) continue;
        
        Node* prey_node = &nodes[all_fish[i].node_id];
        if (!prey_node->active) continue;
        
        float dx = prey_node->x - fish_x;
        float dy = prey_node->y - fish_y;
        float distance_sq = dx * dx + dy * dy;
        
        if (distance_sq <= eating_range_sq) {
            // Successfully caught prey
            float predation_reward = 35.0f + (fish_type->danger_level * 15.0f);
            fish->last_reward += predation_reward;
            
            // Remove prey from simulation
            all_fish[i].active = 0;
            prey_node->active = 0;
            
            // Set eating cooldown
            fish->eating_cooldown = fish_type->eating_cooldown_frames;
            
            return 1;
        }
    }
    
    return 0;
}

// Corpse scavenging for predators
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
    
    // Use spatial grid for efficient corpse searching
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
                // Calculate nutrition from corpse
                int original_fish_type = nodes[node_id].original_fish_type;
                float nutrition_value = calculate_corpse_nutrition_value(original_fish_type);
                
                fish->stomach_contents += nutrition_value;
                fish_internal_add_consumed_nutrition(nutrition_value);
                
                // Reward for scavenging
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

// Defecation system - converts food to environmental nutrients
void fish_defecate(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    // Only herbivores defecate (predators don't contribute to plant growth)
    if (fish_type->is_predator) return;
    
    // Only defecate when stomach is sufficiently full
    if (fish->stomach_contents < 0.7f) {
        return;
    }
    
    // Random chance to defecate each frame (1% when full)
    if ((float)rand() / RAND_MAX > 0.01f) {
        return;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float full_stomach_contents = fish->stomach_contents;
    
    // Return nutrition to environment
    nutrition_add_at_position(fish_node->x, fish_node->y, full_stomach_contents, 
                              STANDARD_DEPLETION_RANGE);
    
    plants_add_environmental_nutrition(full_stomach_contents);
    
    // Empty stomach and update tracking
    fish->stomach_contents = 0.0f;
    fish_internal_add_defecated_nutrition(full_stomach_contents);
    fish->defecation_count++;
    
    // Reward for completing nutrient cycle
    fish->last_reward += full_stomach_contents * 3.0f;
    
    // Trigger reproduction after 3 defecations
    if (fish->defecation_count >= 3) {
        g_parent_fish_id = fish_id;
        fish_reproduce(fish_id);
        fish->defecation_count = 0;
    }
    
    // Plant seeding - herbivores spread seeds when defecating
    if ((float)rand() / RAND_MAX < 0.25f) {
        int plant_type_count = plants_get_type_count();
        if (plant_type_count > 0) {
            int random_plant_type = rand() % plant_type_count;
            
            // Disperse seed away from fish
            float dispersal_angle = fish->heading + M_PI + ((float)rand() / RAND_MAX - 0.5f) * 1.5f;
            float dispersal_distance = 60.0f + ((float)rand() / RAND_MAX) * 40.0f;
            
            float seed_x = fish_node->x + cos(dispersal_angle) * dispersal_distance;
            float seed_y = fish_node->y + sin(dispersal_angle) * dispersal_distance;
            
            // Check world boundaries
            if (seed_x >= WORLD_LEFT && seed_x <= WORLD_RIGHT &&
                seed_y >= WORLD_TOP && seed_y <= WORLD_BOTTOM) {
                
                // Create immune seed (protected from immediate consumption)
                int new_node = simulation_add_seed_node(seed_x, seed_y, random_plant_type);
                if (new_node >= 0) {
                    fish->last_reward += 0.05f;
                }
            }
        }
    }
}

// Herbivore reproduction system with neural network inheritance
void fish_reproduce(int fish_id) {
    Fish* parent_fish = fish_get_by_id(fish_id);
    if (!parent_fish) return;
    
    FishType* fish_type = fish_get_type(parent_fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* parent_node = &nodes[parent_fish->node_id];
    
    // Calculate spawn position near parent
    float spawn_distance = 120.0f;
    float spawn_angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
    float spawn_x = parent_node->x + cos(spawn_angle) * spawn_distance;
    float spawn_y = parent_node->y + sin(spawn_angle) * spawn_distance;
    
    // Ensure spawn position is within world bounds
    spawn_x = fmaxf(WORLD_LEFT + 20, fminf(WORLD_RIGHT - 20, spawn_x));
    spawn_y = fmaxf(WORLD_TOP + 20, fminf(WORLD_BOTTOM - 20, spawn_y));
    
    // Create offspring
    int offspring_id = fish_add(spawn_x, spawn_y, parent_fish->fish_type);
    
    if (offspring_id >= 0) {
        // Large reproduction reward
        parent_fish->last_reward += 150.0f;
        
        // Signal Python system for neural network inheritance
        g_reproduction_notification_pending = 1;
    }
    
    g_parent_fish_id = -1;
}

// Predator reproduction system based on successful hunts
void fish_predator_reproduce(int fish_id) {
    Fish* predator = fish_get_by_id(fish_id);
    if (!predator) return;
    
    FishType* fish_type = fish_get_type(predator->fish_type);
    if (!fish_type || !fish_type->is_predator) return;
    
    // Track kills for each predator
    static int kill_counts[MAX_FISH] = {0};
    
    // Check if predator just made a kill (cooldown was just set)
    if (predator->eating_cooldown == fish_type->eating_cooldown_frames) {
        kill_counts[fish_id]++;
        
        // Reproduce after 3 successful kills
        if (kill_counts[fish_id] >= 3) {
            kill_counts[fish_id] = 0;
            
            Node* nodes = simulation_get_nodes();
            Node* parent_node = &nodes[predator->node_id];
            
            // Calculate spawn position
            float spawn_distance = 150.0f;
            float spawn_angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
            float spawn_x = parent_node->x + cos(spawn_angle) * spawn_distance;
            float spawn_y = parent_node->y + sin(spawn_angle) * spawn_distance;
            
            // Ensure spawn position is within world bounds
            spawn_x = fmaxf(WORLD_LEFT + 20, fminf(WORLD_RIGHT - 20, spawn_x));
            spawn_y = fmaxf(WORLD_TOP + 20, fminf(WORLD_BOTTOM - 20, spawn_y));
            
            // Create offspring with neural network inheritance
            g_parent_fish_id = fish_id;
            int offspring_id = fish_add(spawn_x, spawn_y, predator->fish_type);
            
            if (offspring_id >= 0) {
                predator->last_reward += 200.0f;
            }
            
            g_parent_fish_id = -1;
        }
    }
}

// Neural network inheritance support functions
int fish_get_parent_for_inheritance(void) {
    return g_parent_fish_id;
}

int fish_is_reproduction_pending(void) {
    if (g_reproduction_notification_pending) {
        g_reproduction_notification_pending = 0;
        return 1;
    }
    return 0;
}

// Generic eating attempt wrapper function
int fish_attempt_eating(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return 0;
    
    if (fish_type->is_predator) {
        // Predators try fish first, then corpses
        int ate_fish = fish_attempt_eating_fish(fish_id);
        if (ate_fish) return 1;
        
        return fish_attempt_eating_corpse(fish_id);
    } else {
        // Herbivores only eat plants
        return fish_attempt_eating_plant(fish_id);
    }
}

// Neural network state accessors for Python API
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

// Legacy compatibility functions - maintained for backward compatibility
void fish_set_movement_force(int fish_id, float force_x, float force_y) {
    (void)fish_id; (void)force_x; (void)force_y;
    // No-op: movement now handled by neural networks
}

void fish_clear_movement_force(int fish_id) {
    (void)fish_id;
    // No-op: movement now handled by neural networks
}

void fish_apply_rl_action(int fish_id, float direction_x, float direction_y) {
    (void)fish_id; (void)direction_x; (void)direction_y;
    // No-op: actions now handled by neural network outputs
}

void fish_update_oxygen_system(int fish_id) {
    (void)fish_id;
    // No-op: oxygen handled in reward calculation
}

void fish_update_hunger_system(int fish_id) {
    (void)fish_id;
    // No-op: hunger handled by stomach contents
}

void fish_update_rl_state(int fish_id) {
    // Redirect to vision system
    fish_update_rl_inputs(fish_id);
}

void fish_calculate_environmental_rewards(int fish_id) {
    (void)fish_id;
    // No-op: environmental rewards handled in main reward calculation
}

void fish_calculate_chemoreceptor_rewards(int fish_id) {
    (void)fish_id;
    // No-op: chemoreceptor functionality integrated into vision system
}

void fish_eat_nearby_plants(int fish_id) {
    (void)fish_id;
    // No-op: eating now controlled by neural networks
}

int fish_can_eat_plant(int fish_id, int node_id) {
    (void)fish_id; (void)node_id;
    return 0;  // No-op: eating logic handled in attempt_eating functions
}

// Environmental state accessors
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
    
    // Hunger is inverse of stomach fullness
    return fmaxf(0.0f, 1.0f - fish->stomach_contents);
}

float fish_get_saturation_level(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    return fish ? fish->stomach_contents : 0.0f;
}

float fish_get_reward(int fish_id) {
    return fish_get_last_reward(fish_id);
}