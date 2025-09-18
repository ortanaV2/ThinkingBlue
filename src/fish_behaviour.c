// fish_behaviour.c - Enhanced predator-prey behavior system
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

        // Apply force in heading direction with enhanced movement
        Node* nodes = simulation_get_nodes();
        Node* fish_node = &nodes[fish->node_id];
        
        float force_magnitude = movement_strength * fish_type->max_force;
        
        // Enhanced movement: stronger force for anti-camping
        if (movement_strength > 0.7f) {
            force_magnitude *= 1.3f;  // Boost for high movement commands
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

// Enhanced reward calculation with predator-prey dynamics
void fish_calculate_rl_rewards(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    fish->last_reward = 0.0f;
    
    // Track position history for anti-camping
    static float fish_positions[MAX_FISH][10][2];
    static int position_index[MAX_FISH] = {0};
    static int frame_counter = 0;
    
    frame_counter++;
    
    if (frame_counter % 5 == 0 && fish_id < MAX_FISH) {
        int idx = position_index[fish_id];
        fish_positions[fish_id][idx][0] = fish_node->x;
        fish_positions[fish_id][idx][1] = fish_node->y;
        position_index[fish_id] = (idx + 1) % 10;
    }
    
    float current_speed = sqrt(fish_node->vx * fish_node->vx + fish_node->vy * fish_node->vy);
    
    // 1. Oxygen reward (only for herbivores)
    if (!fish_type->is_predator) {
        float oxygen_level = gas_get_oxygen_at(fish_node->x, fish_node->y);
        float oxygen_reward = oxygen_level * fish_type->oxygen_reward_factor;
        fish->last_reward += oxygen_reward;
    }
    
    // 2. Species-specific proximity rewards
    if (fish_type->is_predator) {
        // Predators: reward for proximity to prey fish
        float distance_to_prey = fish_get_distance_to_nearest_foreign_fish(fish_id);
        if (distance_to_prey < 1000.0f && current_speed > 0.5f) {
            float proximity_factor = 1.0f - (distance_to_prey / 1000.0f);
            float predator_reward = fish_type->proximity_reward_factor * proximity_factor * 3.0f;
            fish->last_reward += predator_reward;
        }
    } else {
        // Herbivores: reward for proximity to plants (when moving)
        float distance_to_plant = fish_get_distance_to_nearest_plant(fish_id);
        if (distance_to_plant < 2000.0f && current_speed > 0.3f) {
            float proximity_base = fish_type->proximity_reward_factor;
            float distance_factor;
            
            if (distance_to_plant < 50.0f) {
                distance_factor = 12.0f;  // Reduced from 15.0f
            } else if (distance_to_plant < 100.0f) {
                distance_factor = 8.0f * exp(-(distance_to_plant - 50.0f) / 50.0f);
            } else if (distance_to_plant < 200.0f) {
                distance_factor = 5.0f * exp(-(distance_to_plant - 100.0f) / 100.0f);
            } else {
                distance_factor = 2.0f * exp(-(distance_to_plant - 200.0f) / 400.0f);
            }
            
            float speed_bonus = fminf(current_speed / fish_type->max_speed, 1.0f);
            float proximity_reward = proximity_base * distance_factor * speed_bonus;
            fish->last_reward += proximity_reward;
        }
    }
    
    // 3. Enhanced movement rewards with stronger anti-camping
    float movement_reward = 0.0f;
    
    if (current_speed > 2.0f) {
        // Strong reward for fast movement
        movement_reward = 0.025f * fminf(current_speed / fish_type->max_speed, 1.0f);
    } else if (current_speed > 1.0f) {
        // Good reward for active movement
        movement_reward = 0.015f * fminf(current_speed / fish_type->max_speed, 1.0f);
    } else if (current_speed > 0.3f) {
        // Small reward for slow movement
        movement_reward = 0.005f * (current_speed / fish_type->max_speed);
    } else {
        // Strong punishment for being stationary (anti-camping)
        movement_reward = -0.03f;  // Increased punishment
    }
    
    fish->last_reward += movement_reward;
    
    // 4. Enhanced anti-camping system
    if (fish_id < MAX_FISH && frame_counter > 50) {
        float total_distance_moved = 0.0f;
        int valid_positions = 0;
        
        for (int i = 0; i < 9; i++) {
            int curr_idx = (position_index[fish_id] + i) % 10;
            int next_idx = (position_index[fish_id] + i + 1) % 10;
            
            float dx = fish_positions[fish_id][next_idx][0] - fish_positions[fish_id][curr_idx][0];
            float dy = fish_positions[fish_id][next_idx][1] - fish_positions[fish_id][curr_idx][1];
            float distance = sqrt(dx * dx + dy * dy);
            
            total_distance_moved += distance;
            valid_positions++;
        }
        
        if (valid_positions > 0) {
            float avg_movement = total_distance_moved / valid_positions;
            
            if (avg_movement < 8.0f) {
                fish->last_reward -= 0.04f;  // Very strong camping punishment
            } else if (avg_movement > 80.0f) {
                fish->last_reward += 0.01f;  // Exploration bonus
            }
        }
    }
    
    // 5. Enhanced predator-prey interaction rewards with learning incentives
    float danger_level = fish->rl_inputs[6];  // From foreign fish detection
    float foreign_fish_magnitude = sqrt(fish->rl_inputs[4] * fish->rl_inputs[4] + 
                                        fish->rl_inputs[5] * fish->rl_inputs[5]);
    
    if (fabs(danger_level) > 0.1f && foreign_fish_magnitude > 0.1f) {
        if (danger_level < 0.0f) {
            // DANGEROUS PREDATOR NEARBY - Critical survival situation
            float danger_intensity = -danger_level;  // Convert to positive (0.0 to 1.0)
            float foreign_fish_distance = 1.0f - foreign_fish_magnitude;  // Rough distance estimate
            
            if (foreign_fish_distance < 0.3f) {
                // VERY CLOSE PREDATOR - Emergency situation!
                if (current_speed > 2.0f) {
                    // BIG REWARD for fleeing fast when predator is close
                    float flee_reward = 0.15f * danger_intensity * (current_speed / fish_type->max_speed);
                    fish->last_reward += flee_reward;
                    
                    // Extra bonus for turning away from predator
                    float fish_vec_x = fish->rl_inputs[4];
                    float fish_vec_y = fish->rl_inputs[5];
                    float heading_x = cos(fish->heading);
                    float heading_y = sin(fish->heading);
                    
                    // Dot product: negative means fleeing, positive means approaching
                    float approach_factor = fish_vec_x * heading_x + fish_vec_y * heading_y;
                    if (approach_factor < -0.3f) {  // Fleeing in opposite direction
                        fish->last_reward += 0.08f * danger_intensity;
                    }
                } else {
                    // SEVERE PUNISHMENT for not fleeing when predator is very close
                    float death_risk_punishment = -0.25f * danger_intensity;
                    fish->last_reward += death_risk_punishment;
                }
            } else if (foreign_fish_distance < 0.6f) {
                // MODERATE DISTANCE PREDATOR - Time to start fleeing
                if (current_speed > 1.5f) {
                    // Good reward for early escape
                    float early_flee_reward = 0.08f * danger_intensity * (current_speed / fish_type->max_speed);
                    fish->last_reward += early_flee_reward;
                } else if (current_speed < 0.5f) {
                    // Punishment for ignoring nearby predator
                    fish->last_reward -= 0.12f * danger_intensity;
                }
            } else {
                // DISTANT PREDATOR - Awareness reward
                if (current_speed > 1.0f) {
                    // Small reward for cautious movement when predator detected
                    fish->last_reward += 0.03f * danger_intensity;
                } else {
                    // Light punishment for complacency
                    fish->last_reward -= 0.04f * danger_intensity;
                }
            }
            
            // Additional punishment for trying to eat when predator nearby
            if (fish->eating_mode && foreign_fish_distance < 0.5f) {
                fish->last_reward -= 0.20f * danger_intensity;  // Don't eat when in danger!
            }
            
        } else if (fish_type->is_predator && danger_level > 0.0f) {
            // PREDATOR BEHAVIOR - Hunting weaker fish
            float prey_weakness = danger_level;  // How much weaker the prey is
            float prey_distance = 1.0f - foreign_fish_magnitude;
            
            if (prey_distance < 0.2f) {
                // VERY CLOSE TO PREY - Attack time!
                if (fish->eating_mode) {
                    // Big reward for attacking when close to prey
                    fish->last_reward += 0.12f * prey_weakness;
                } else if (current_speed > 1.5f) {
                    // Reward for closing in fast
                    fish->last_reward += 0.08f * prey_weakness;
                }
            } else if (prey_distance < 0.5f) {
                // MODERATE DISTANCE TO PREY - Pursue
                if (current_speed > 1.0f) {
                    // Reward for pursuit
                    float pursuit_reward = 0.06f * prey_weakness * (current_speed / fish_type->max_speed);
                    fish->last_reward += pursuit_reward;
                    
                    // Extra reward for heading toward prey
                    float prey_vec_x = fish->rl_inputs[4];
                    float prey_vec_y = fish->rl_inputs[5];
                    float heading_x = cos(fish->heading);
                    float heading_y = sin(fish->heading);
                    
                    float pursuit_factor = prey_vec_x * heading_x + prey_vec_y * heading_y;
                    if (pursuit_factor > 0.3f) {  // Chasing prey
                        fish->last_reward += 0.04f * prey_weakness;
                    }
                }
            } else {
                // DISTANT PREY - Stalking reward
                if (current_speed > 0.8f && current_speed < fish_type->max_speed * 0.7f) {
                    // Reward for controlled approach (not too slow, not too fast)
                    fish->last_reward += 0.02f * prey_weakness;
                }
            }
        }
    }
    
    // 6. Survival bonus for prey fish that successfully avoid predators
    static float last_danger_levels[MAX_FISH] = {0.0f};
    static int danger_frames[MAX_FISH] = {0};
    
    if (fish_id < MAX_FISH && !fish_type->is_predator) {
        if (danger_level < -0.3f) {
            // Currently in danger
            danger_frames[fish_id]++;
            last_danger_levels[fish_id] = danger_level;
        } else if (last_danger_levels[fish_id] < -0.3f && danger_frames[fish_id] > 0) {
            // Just escaped from danger!
            float survival_bonus = 0.1f * (-last_danger_levels[fish_id]) * 
                                   fminf(danger_frames[fish_id] / 30.0f, 3.0f);  // Up to 3x bonus
            fish->last_reward += survival_bonus;
            
            printf("Fish %d survived predator encounter! Bonus: %.3f (danger was %.2f for %d frames)\n",
                   fish_id, survival_bonus, last_danger_levels[fish_id], danger_frames[fish_id]);
            
            // Reset tracking
            danger_frames[fish_id] = 0;
            last_danger_levels[fish_id] = 0.0f;
        }
    }
    
    // 7. Eating system with enhanced predator-prey awareness
    static int last_eat_attempt[MAX_FISH] = {0};
    static int eat_spam_counter[MAX_FISH] = {0};
    
    if (fish->eating_mode) {
        if (fish_id < MAX_FISH) {
            int frames_since_last_eat = frame_counter - last_eat_attempt[fish_id];
            
            if (frames_since_last_eat < 20) {  // Eating too frequently
                eat_spam_counter[fish_id]++;
                float spam_punishment = -0.03f * (eat_spam_counter[fish_id] * eat_spam_counter[fish_id]);
                fish->last_reward += spam_punishment;
            } else {
                eat_spam_counter[fish_id] = 0;
            }
            
            last_eat_attempt[fish_id] = frame_counter;
        }
        
        // Check for nearby danger before eating
        if (!fish_type->is_predator && danger_level < -0.2f && foreign_fish_magnitude > 0.3f) {
            // Prey fish trying to eat while predator is nearby - additional punishment
            fish->last_reward -= 0.15f * (-danger_level);
        }
        
        int ate_something = 0;
        
        if (fish_type->is_predator) {
            ate_something = fish_attempt_eating_fish(fish_id);
        } else {
            ate_something = fish_attempt_eating_plant(fish_id);
        }
        
        if (!ate_something) {
            // Failed eating attempt
            float failed_eat_punishment = fish_type->eat_punishment * 2.5f;  // Reduced from 3.0f
            fish->last_reward += failed_eat_punishment;
        }
    }
    
    // 8. Enhanced exploration and movement rewards
    if (current_speed > 1.2f) {
        fish->last_reward += 0.005f;  // Increased from 0.003f
    }
    
    // 9. Reduced base survival reward
    fish->last_reward += 0.0001f;
    
    fish->total_reward += fish->last_reward;
}

// Attempt to eat plants (herbivores only)
int fish_attempt_eating_plant(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || fish_type->is_predator) return 0;  // Predators can't eat plants
    
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
                int plant_type = nodes[node_id].plant_type;
                float nutrition_value = calculate_plant_nutrition_value(plant_type);
                
                // Add nutrition to stomach
                fish->stomach_contents += nutrition_value;
                fish_internal_add_consumed_nutrition(nutrition_value);
                
                // Large eating reward
                float eating_reward = nutrition_value * 30.0f;  // Increased reward
                fish->last_reward += eating_reward;
                
                // Remove eaten plant
                nodes[node_id].active = 0;
                nodes[node_id].can_grow = 0;
                
                fish->last_eating_frame = simulation_get_frame_counter();
                
                PlantType* pt = plants_get_type(plant_type);
                printf("Fish %d ate %s (nutrition: %.3f, reward: %.2f)\n", 
                       fish_id, pt ? pt->name : "plant", nutrition_value, eating_reward);
                
                return 1;
            }
        }
    }
    
    return 0;
}

// NEW: Attempt to eat fish (predators only)
int fish_attempt_eating_fish(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || !fish_type->is_predator) return 0;  // Only predators can eat fish
    
    // Check eating cooldown
    if (fish->eating_cooldown > 0) {
        // Can't eat yet, but give small reward for trying at right time
        fish->last_reward += 0.005f;
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
        if (i == fish_id) continue;  // Can't eat self
        if (all_fish[i].fish_type == fish->fish_type) continue;  // Can't eat same species
        
        FishType* prey_type = fish_get_type(all_fish[i].fish_type);
        if (!prey_type) continue;
        
        // Can only eat fish with lower danger level
        if (prey_type->danger_level >= fish_type->danger_level) continue;
        
        Node* prey_node = &nodes[all_fish[i].node_id];
        if (!prey_node->active) continue;
        
        float dx = prey_node->x - fish_x;
        float dy = prey_node->y - fish_y;
        float distance_sq = dx * dx + dy * dy;
        
        if (distance_sq <= eating_range_sq) {
            // Successfully caught prey
            
            // Large predator reward
            float predation_reward = 50.0f + (fish_type->danger_level * 20.0f);
            fish->last_reward += predation_reward;
            
            // Remove prey fish
            all_fish[i].active = 0;
            prey_node->active = 0;
            
            // Set eating cooldown
            fish->eating_cooldown = fish_type->eating_cooldown_frames;
            
            printf("Predator fish %d caught prey fish %d (reward: %.1f, cooldown: %d)\n", 
                   fish_id, i, predation_reward, fish->eating_cooldown);
            
            return 1;
        }
    }
    
    return 0;
}

// Enhanced defecation with reproduction system
void fish_defecate(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    // Only herbivores defecate (predators don't use nutrition system)
    if (fish_type->is_predator) return;
    
    if (fish->stomach_contents < 0.6f) {
        return;
    }
    
    if ((float)rand() / RAND_MAX > 0.01f) {
        return;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float defecation_amount = fish->stomach_contents;
    
    // Add nutrition back to environment
    nutrition_add_at_position(fish_node->x, fish_node->y, defecation_amount, 80.0f);
    
    fish->stomach_contents = 0.0f;
    fish_internal_add_defecated_nutrition(defecation_amount);
    
    // Track defecations for reproduction
    fish->defecation_count++;
    
    // Reward for completing nutrition cycle
    fish->last_reward += defecation_amount * 2.5f;
    
    printf("Fish %d defecated %.3f nutrition (count: %d)\n", 
           fish_id, defecation_amount, fish->defecation_count);
    
    // Reproduction after 2 defecations
    if (fish->defecation_count >= 2) {
        fish_reproduce(fish_id);
        fish->defecation_count = 0;  // Reset counter
    }
    
    // Plant seeding chance
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
                    fish->last_reward += 0.05f;
                }
            }
        }
    }
}

// NEW: Fish reproduction system
void fish_reproduce(int fish_id) {
    Fish* parent_fish = fish_get_by_id(fish_id);
    if (!parent_fish) return;
    
    Node* nodes = simulation_get_nodes();
    Node* parent_node = &nodes[parent_fish->node_id];
    
    // Spawn offspring near parent
    float spawn_distance = 100.0f;
    float spawn_angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
    float spawn_x = parent_node->x + cos(spawn_angle) * spawn_distance;
    float spawn_y = parent_node->y + sin(spawn_angle) * spawn_distance;
    
    // Clamp to world bounds
    if (spawn_x < WORLD_LEFT) spawn_x = WORLD_LEFT + 10.0f;
    if (spawn_x > WORLD_RIGHT) spawn_x = WORLD_RIGHT - 10.0f;
    if (spawn_y < WORLD_TOP) spawn_y = WORLD_TOP + 10.0f;
    if (spawn_y > WORLD_BOTTOM) spawn_y = WORLD_BOTTOM - 10.0f;
    
    int offspring_id = fish_add(spawn_x, spawn_y, parent_fish->fish_type);
    
    if (offspring_id >= 0) {
        // Large reproduction reward
        parent_fish->last_reward += 100.0f;
        
        FishType* ft = fish_get_type(parent_fish->fish_type);
        printf("Fish %d (%s) reproduced! Offspring %d spawned at (%.1f, %.1f)\n",
               fish_id, ft ? ft->name : "?", offspring_id, spawn_x, spawn_y);
    }
}

// Wrapper function for backward compatibility
int fish_attempt_eating(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return 0;
    
    if (fish_type->is_predator) {
        return fish_attempt_eating_fish(fish_id);
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