// fish_update.c - Main fish update loop for RL system (with debug)
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
        
        if (node_id < 0 || node_id >= node_count) {
            printf("Warning: Fish %d has invalid node_id %d\n", i, node_id);
            continue;
        }
        if (!nodes[node_id].active) {
            printf("Warning: Fish %d node %d is not active\n", i, node_id);
            continue;
        }
        
        FishType* fish_type = &fish_types[fish->fish_type];
        Node* node = &nodes[node_id];
        
        // Reset frame reward
        fish->last_reward = 0.0f;
        
        // Update RL inputs (plant vector + oxygen level)
        fish_update_rl_inputs(i);
        
        // DEBUG: Check if RL outputs are being set for all fish
        static int debug_frame_counters[MAX_FISH] = {0};
        if (i < MAX_FISH && (debug_frame_counters[i]++ % 30) == 0) {  // Every second for all fish
            printf("DEBUG Fish %d: RL outputs = (%.3f, %.3f, %.3f), heading = %.2f°, speed = %.2f\n",
                   i, fish->rl_outputs[0], fish->rl_outputs[1], fish->rl_outputs[2], 
                   fish->heading * 180.0f / M_PI, sqrt(node->vx * node->vx + node->vy * node->vy));
        }
        
        // Apply RL outputs (turn, move, eat) - this calls the behavior system
        fish_apply_rl_outputs(i);
        
        // Debug: Check if movement was applied for first 3 fish
        if (i < 3 && (debug_frame_counters[i] % 30) == 1) {
            printf("DEBUG Fish %d: velocity = (%.3f, %.3f), eating_mode = %d\n",
                   i, node->vx, node->vy, fish->eating_mode);
        }
        
        // If not in eating mode, apply physics
        if (!fish->eating_mode) {
            // Apply minimal flow influence
            float flow_x, flow_y;
            flow_get_vector_at(node->x, node->y, &flow_x, &flow_y);
            
            float flow_influence = fish_type->flow_sensitivity * 0.05f; // Very minimal
            node->vx += flow_x * flow_influence;
            node->vy += flow_y * flow_influence;
            
            // Apply drag
            node->vx *= WATER_DRAG;
            node->vy *= WATER_DRAG;
            
            // Update position
            node->x += node->vx;
            node->y += node->vy;
            
            // World bounds collision
            if (node->x < WORLD_LEFT) {
                node->x = WORLD_LEFT;
                node->vx = 0;
                fish->last_reward -= 0.01f;  // Boundary penalty
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
            // In eating mode - stop movement
            node->vx *= 0.1f;  // Rapid deceleration
            node->vy *= 0.1f;
        }
        
        // Calculate RL rewards
        fish_calculate_rl_rewards(i);
        
        // Try to defecate
        fish_defecate(i);
        
        // Age tracking
        fish->age++;
        
        // DEBUG: If no movement for any fish, give it some default movement
        if (fabs(node->vx) < 0.1f && fabs(node->vy) < 0.1f && !fish->eating_mode) {
            // Give stuck fish some test movement
            if ((current_frame % (90 + i * 15)) == 0) {  // Staggered timing for each fish
                printf("DEBUG: Fish %d appears stuck, applying test force\n", i);
                fish->heading += 0.3f + i * 0.1f;  // Unique turn for each fish
                float test_force = fish_type->max_force * 0.5f;
                node->vx += cos(fish->heading) * test_force;
                node->vy += sin(fish->heading) * test_force;
            }
        }
    }
    
    // Debug output every 10 seconds
    static int last_debug_frame = 0;
    if (current_frame - last_debug_frame >= 600) {
        last_debug_frame = current_frame;
        
        float total_consumed = fish_get_total_nutrition_consumed();
        float total_defecated = fish_get_total_nutrition_defecated();
        float balance = total_consumed - total_defecated;
        float total_stomach = 0.0f;
        float avg_reward = 0.0f;
        float avg_oxygen = 0.0f;
        float avg_movement_output = 0.0f;
        int active_fish = 0;
        int eating_mode_fish = 0;
        
        for (int i = 0; i < fish_count; i++) {
            if (fish_array[i].active) {
                total_stomach += fish_array[i].stomach_contents;
                avg_reward += fish_array[i].last_reward;
                avg_oxygen += fish_array[i].rl_inputs[2];  // Oxygen input
                avg_movement_output += fish_array[i].rl_outputs[1];  // Movement strength output
                
                if (fish_array[i].eating_mode) eating_mode_fish++;
                
                active_fish++;
            }
        }
        
        if (active_fish > 0) {
            avg_reward /= active_fish;
            avg_oxygen /= active_fish;
            avg_movement_output /= active_fish;
        }
        
        printf("\n=== RL FISH STATUS Frame %d ===\n", current_frame);
        printf("Active fish: %d\n", active_fish);
        printf("Fish in eating mode: %d\n", eating_mode_fish);
        printf("Average oxygen level: %.3f\n", avg_oxygen);
        printf("Average movement output: %.3f\n", avg_movement_output);
        printf("Average last reward: %.4f\n", avg_reward);
        printf("Total consumed: %.4f\n", total_consumed);
        printf("Total defecated: %.4f\n", total_defecated);
        printf("In fish stomachs: %.4f\n", total_stomach);
        printf("Nutrition balance: %.4f\n", balance);
        
        // Show sample fish details with all RL data (first 3 fish)
        for (int i = 0; i < fminf(3, fish_count); i++) {
            if (fish_array[i].active) {
                Fish* fish = &fish_array[i];
                Node* node = &nodes[fish->node_id];
                
                printf("Fish %d: pos(%.1f,%.1f), heading=%.1f°, "
                       "inputs=(%.2f,%.2f,%.2f,%.2f), outputs=(%.2f,%.2f,%.2f), "
                       "vel=(%.2f,%.2f), reward=%.3f\n",
                       i, node->x, node->y, fish->heading * 180.0f / M_PI,
                       fish->rl_inputs[0], fish->rl_inputs[1], fish->rl_inputs[2], fish->rl_inputs[3],
                       fish->rl_outputs[0], fish->rl_outputs[1], fish->rl_outputs[2],
                       node->vx, node->vy, fish->last_reward);
            }
        }
        
        printf("============================================\n\n");
    }
}