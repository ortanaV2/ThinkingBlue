// fish_update.c - Robust fish update with optimized ID system
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"
#include "flow.h"

// Forward declaration for efficient iteration
int fish_get_highest_slot(void);

// Update corpse decay system
void fish_update_corpses(void) {
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    int corpses_decayed = 0;
    
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        if (!nodes[i].is_corpse) continue;
        
        // Decay timer countdown
        nodes[i].corpse_decay_timer--;
        
        // Remove corpse when timer reaches zero
        if (nodes[i].corpse_decay_timer <= 0) {
            nodes[i].active = 0;
            corpses_decayed++;
        }
    }
    
    // Debug output for corpse decay
    static int last_corpse_log = 0;
    int current_frame = simulation_get_frame_counter();
    if (corpses_decayed > 0 && (current_frame - last_corpse_log) > 300) {
        printf("Corpse decay: %d corpses naturally decayed\n", corpses_decayed);
        last_corpse_log = current_frame;
    }
}

// OPTIMIZED: Main fish update function with efficient iteration
void fish_update(void) {
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    int current_frame = simulation_get_frame_counter();
    
    Fish* fish_array = fish_internal_get_array();
    FishType* fish_types = fish_internal_get_types();
    
    // Update corpse decay system
    fish_update_corpses();
    
    // Track deaths and validation errors
    int deaths_this_frame = 0;
    int validation_errors = 0;
    int fish_updated = 0;
    
    // OPTIMIZED: Only iterate up to highest used slot + 1
    int highest_slot = fish_get_highest_slot();
    int iteration_limit = (highest_slot >= 0) ? (highest_slot + 1) : 0;
    
    for (int i = 0; i < iteration_limit; i++) {
        // Skip inactive fish
        if (!fish_array[i].active) continue;
        
        Fish* fish = &fish_array[i];
        int node_id = fish->node_id;
        
        // ROBUST: Validate node ID
        if (node_id < 0 || node_id >= node_count) {
            printf("ERROR: Fish %d has invalid node_id %d (max=%d), deactivating\n", 
                   i, node_id, node_count - 1);
            fish->active = 0;
            validation_errors++;
            continue;
        }
        
        if (!nodes[node_id].active) {
            printf("ERROR: Fish %d node %d is inactive, deactivating fish\n", i, node_id);
            fish->active = 0;
            validation_errors++;
            continue;
        }
        
        // ROBUST: Validate fish type
        if (fish->fish_type < 0 || fish->fish_type >= fish_get_type_count()) {
            printf("ERROR: Fish %d has invalid fish_type %d (max=%d), deactivating\n", 
                   i, fish->fish_type, fish_get_type_count() - 1);
            fish->active = 0;
            validation_errors++;
            continue;
        }
        
        FishType* fish_type = &fish_types[fish->fish_type];
        Node* node = &nodes[node_id];
        
        // Check for death from aging
        if (fish_should_die_from_age(i)) {
            fish->active = 0;
            node->active = 0;
            deaths_this_frame++;
            continue;
        }
        
        // CORE FISH UPDATES
        
        // Reset frame reward
        fish->last_reward = 0.0f;
        
        // Update RL inputs
        fish_update_rl_inputs(i);
        
        // Apply RL outputs
        fish_apply_rl_outputs(i);
        
        // Physics and movement
        if (!fish->eating_mode) {
            // Apply minimal flow influence
            float flow_x, flow_y;
            flow_get_vector_at(node->x, node->y, &flow_x, &flow_y);
            
            float flow_influence = fish_type->flow_sensitivity * 0.03f;
            node->vx += flow_x * flow_influence;
            node->vy += flow_y * flow_influence;
            
            // Apply drag
            node->vx *= WATER_DRAG;
            node->vy *= WATER_DRAG;
            
            // Update position
            node->x += node->vx;
            node->y += node->vy;
            
            // World bounds collision with penalty
            if (node->x < WORLD_LEFT) {
                node->x = WORLD_LEFT;
                node->vx = 0;
                fish->last_reward -= 0.01f;
            }
            if (node->x > WORLD_RIGHT) {
                node->x = WORLD_RIGHT;
                node->vx = 0;
                fish->last_reward -= 0.01f;
            }
            if (node->y < WORLD_TOP) {
                node->y = WORLD_TOP;
                node->vy = 0;
                fish->last_reward -= 0.01f;
            }
            if (node->y > WORLD_BOTTOM) {
                node->y = WORLD_BOTTOM;
                node->vy = 0;
                fish->last_reward -= 0.01f;
            }
        } else {
            // In eating mode - reduce movement
            node->vx *= 0.2f;
            node->vy *= 0.2f;
        }
        
        // Calculate RL rewards
        fish_calculate_rl_rewards(i);
        
        // Defecation (herbivores only)
        fish_defecate(i);
        
        // Predator reproduction check
        if (fish_type->is_predator) {
            fish_predator_reproduce(i);
        }
        
        // Age tracking (increment age every frame)
        fish->age++;
        
        fish_updated++;
    }
    
    // Error reporting
    if (validation_errors > 0) {
        printf("WARNING: Fixed %d fish validation errors this frame\n", validation_errors);
    }
    
    // Debug output every 15 seconds with robust statistics
    static int last_debug_frame = 0;
    if (current_frame - last_debug_frame >= 450) {
        last_debug_frame = current_frame;
        
        float total_consumed = fish_get_total_nutrition_consumed();
        float total_defecated = fish_get_total_nutrition_defecated();
        float balance = total_consumed - total_defecated;
        int active_fish = 0;
        int eating_mode_fish = 0;
        int predator_count = 0;
        int herbivore_count = 0;
        int old_fish_count = 0;
        int active_corpses = 0;
        
        // Count active corpses
        for (int i = 0; i < node_count; i++) {
            if (nodes[i].active && nodes[i].is_corpse) {
                active_corpses++;
            }
        }
        
        // ROBUST: Count fish with validation
        for (int i = 0; i <= highest_slot && i < MAX_FISH; i++) {
            if (fish_array[i].active) {
                // Validate before accessing fish_type
                if (fish_array[i].fish_type >= 0 && fish_array[i].fish_type < fish_get_type_count()) {
                    FishType* ft = &fish_types[fish_array[i].fish_type];
                    if (ft->is_predator) {
                        predator_count++;
                    } else {
                        herbivore_count++;
                    }
                    
                    if (fish_array[i].eating_mode) eating_mode_fish++;
                    
                    // Check if fish is old (past 75% of max age)
                    int age = current_frame - fish_array[i].birth_frame;
                    if (age > ft->max_age * 0.75f) {
                        old_fish_count++;
                    }
                }
                active_fish++;
            }
        }
        
        printf("\n=== FISH ECOSYSTEM STATUS (ROBUST ID SYSTEM) Frame %d ===\n", current_frame);
        printf("Active fish: %d (%d herbivores, %d predators) - Updated: %d\n", 
               active_fish, herbivore_count, predator_count, fish_updated);
        printf("Fish slots used: 0-%d (highest slot: %d)\n", highest_slot, highest_slot);
        printf("Fish in eating mode: %d\n", eating_mode_fish);
        printf("Old fish (>75%% max age): %d\n", old_fish_count);
        printf("Active corpses: %d\n", active_corpses);
        printf("Total deaths from aging: %d\n", fish_get_total_deaths_from_age());
        printf("Total corpses created: %d\n", fish_get_total_corpses_created());
        printf("Total corpses eaten: %d\n", fish_get_total_corpses_eaten());
        printf("Nutrition consumed: %.4f\n", total_consumed);
        printf("Nutrition defecated: %.4f\n", total_defecated);
        printf("Nutrition balance: %.4f\n", balance);
        
        // Show sample fish with validation - ROBUST
        int samples_shown = 0;
        for (int i = 0; i <= highest_slot && i < MAX_FISH && samples_shown < 3; i++) {
            if (fish_array[i].active) {
                Fish* fish = &fish_array[i];
                
                // Validate all data before displaying
                if (fish->node_id >= 0 && fish->node_id < node_count && 
                    fish->fish_type >= 0 && fish->fish_type < fish_get_type_count()) {
                    
                    Node* node = &nodes[fish->node_id];
                    FishType* ft = &fish_types[fish->fish_type];
                    
                    if (node->active) {
                        float speed = sqrt(node->vx * node->vx + node->vy * node->vy);
                        
                        int age = current_frame - fish->birth_frame;
                        float age_minutes = age / (TARGET_FPS * 60.0f);
                        float max_age_minutes = ft->max_age / (TARGET_FPS * 60.0f);
                        float age_percentage = (float)age / (float)ft->max_age * 100.0f;
                        
                        printf("Fish %d (%s): pos(%.0f,%.0f), speed=%.1f, age=%.1f/%.1f min (%.0f%%), "
                               "outputs=(%.2f,%.2f,%.2f), reward=%.3f\n",
                               i, ft->name, node->x, node->y, speed, age_minutes, max_age_minutes, age_percentage,
                               fish->rl_outputs[0], fish->rl_outputs[1], fish->rl_outputs[2], fish->last_reward);
                        samples_shown++;
                    }
                } else {
                    printf("Fish %d: VALIDATION ERROR (node=%d, type=%d)\n", 
                           i, fish->node_id, fish->fish_type);
                    samples_shown++;
                }
            }
        }
        
        if (deaths_this_frame > 0) {
            printf("Deaths this frame: %d\n", deaths_this_frame);
        }
        
        if (validation_errors > 0) {
            printf("Validation errors fixed: %d\n", validation_errors);
        }
        
        printf("Neural networks learning with robust ID tracking...\n");
        printf("==========================================\n\n");
    }
}