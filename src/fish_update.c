// fish_update.c - Main fish update loop with enhanced reproduction tracking
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"
#include "flow.h"

// Main fish update function
void fish_update(void) {
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    int current_frame = simulation_get_frame_counter();
    
    Fish* fish_array = fish_internal_get_array();
    FishType* fish_types = fish_internal_get_types();
    int fish_count = fish_get_count();
    
    for (int i = 0; i < fish_count; i++) {
        if (!fish_array[i].active) continue;
        
        Fish* fish = &fish_array[i];
        int node_id = fish->node_id;
        
        if (node_id < 0 || node_id >= node_count) continue;
        if (!nodes[node_id].active) continue;
        
        FishType* fish_type = &fish_types[fish->fish_type];
        Node* node = &nodes[node_id];
        
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
        
        // Age tracking
        fish->age++;
    }
    
    // Debug output every 15 seconds
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
        
        for (int i = 0; i < fish_count; i++) {
            if (fish_array[i].active) {
                FishType* ft = &fish_types[fish_array[i].fish_type];
                if (ft->is_predator) {
                    predator_count++;
                } else {
                    herbivore_count++;
                }
                
                if (fish_array[i].eating_mode) eating_mode_fish++;
                active_fish++;
            }
        }
        
        printf("\n=== FISH ECOSYSTEM STATUS Frame %d ===\n", current_frame);
        printf("Active fish: %d (%d herbivores, %d predators)\n", active_fish, herbivore_count, predator_count);
        printf("Fish in eating mode: %d\n", eating_mode_fish);
        printf("Nutrition consumed: %.4f\n", total_consumed);
        printf("Nutrition defecated: %.4f\n", total_defecated);
        printf("Nutrition balance: %.4f\n", balance);
        
        // Show sample fish with neural network control
        int samples_shown = 0;
        for (int i = 0; i < fish_count && samples_shown < 3; i++) {
            if (fish_array[i].active) {
                Fish* fish = &fish_array[i];
                Node* node = &nodes[fish->node_id];
                FishType* ft = &fish_types[fish->fish_type];
                
                float speed = sqrt(node->vx * node->vx + node->vy * node->vy);
                
                printf("Fish %d (%s): pos(%.0f,%.0f), speed=%.1f, "
                       "outputs=(%.2f,%.2f,%.2f), reward=%.3f, total=%.1f\n",
                       i, ft->name, node->x, node->y, speed,
                       fish->rl_outputs[0], fish->rl_outputs[1], fish->rl_outputs[2],
                       fish->last_reward, fish->total_reward);
                samples_shown++;
            }
        }
        
        printf("Neural networks learning through experience...\n");
        printf("==========================================\n\n");
    }
}