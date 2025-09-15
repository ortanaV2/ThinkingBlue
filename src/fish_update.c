// fish_update.c - Main fish update loop and physics
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
        
        // Reset frame rewards
        fish->last_reward = 0.0f;
        
        Node* node = &nodes[node_id];
        
        // Direct velocity setting with strong fish movement
        float target_speed = fish_type->max_speed;
        
        // Apply fish movement force directly (PRIMARY movement)
        if (fabs(fish->movement_force_x) > 0.01f || fabs(fish->movement_force_y) > 0.01f) {
            // Fish-controlled movement
            node->vx = fish->movement_force_x * target_speed;
            node->vy = fish->movement_force_y * target_speed;
            
            // Movement reward for active movement
            fish->last_reward += 0.005f;
        } else {
            // FIXED: Much calmer default movement
            // Gradually slow down instead of random movement
            node->vx *= 0.95f;  // Gradual deceleration
            node->vy *= 0.95f;
            
            // Very gentle random nudge only if nearly stationary
            float current_speed = sqrt(node->vx * node->vx + node->vy * node->vy);
            if (current_speed < 0.5f) {
                float gentle_angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
                float gentle_speed = target_speed * 0.1f; // Very gentle 10% speed
                
                node->vx += cos(gentle_angle) * gentle_speed;
                node->vy += sin(gentle_angle) * gentle_speed;
            }
            
            // Small penalty for not having direction
            fish->last_reward -= 0.005f;  // Reduced penalty
        }
        
        // MINIMAL flow influence (secondary, subtle effect)
        float flow_x, flow_y;
        flow_get_vector_at(node->x, node->y, &flow_x, &flow_y);
        
        float flow_influence = fish_type->flow_sensitivity * 0.1f; // Reduced further
        node->vx += flow_x * flow_influence;
        node->vy += flow_y * flow_influence;
        
        // Speed bonus for moving at good speed
        float current_speed = sqrt(node->vx * node->vx + node->vy * node->vy);
        if (current_speed > fish_type->max_speed * 0.7f) {
            fish->last_reward += 0.003f;
        }
        
        // Digestion - convert stomach contents to energy
        if (fish->stomach_contents > 0.0f) {
            float digestion_amount = fish_type->digestion_rate;
            
            if (digestion_amount > fish->stomach_contents) {
                digestion_amount = fish->stomach_contents;
            }
            
            fish->stomach_contents -= digestion_amount;
            fish->energy += digestion_amount;
            
            if (fish->energy > 1.0f) fish->energy = 1.0f;
            if (fish->stomach_contents < 0.0f) fish->stomach_contents = 0.0f;
        }
        
        // Enhanced RL state updates
        fish_update_rl_state(i);
        fish_calculate_environmental_rewards(i);
        fish_calculate_chemoreceptor_rewards(i);
        
        // Try to eat
        fish_eat_nearby_plants(i);
        
        // Try to defecate
        fish_defecate(i);
        
        // Boundary penalties
        float boundary_distance = 1000.0f;
        if (node->x < WORLD_LEFT + boundary_distance || node->x > WORLD_RIGHT - boundary_distance ||
            node->y < WORLD_TOP + boundary_distance || node->y > WORLD_BOTTOM - boundary_distance) {
            fish->last_reward -= 0.025f;
        }
        
        // Age and energy decay
        fish->age++;
        fish->energy -= 0.00005f;
        if (fish->energy < 0.0f) fish->energy = 0.0f;
        
        // Base survival reward
        fish->last_reward += 0.002f;
    }
    
    // Debug output every 10 seconds
    static int last_debug_frame = 0;
    if (current_frame - last_debug_frame >= 600) {
        last_debug_frame = current_frame;
        
        float total_consumed = fish_get_total_nutrition_consumed();
        float total_defecated = fish_get_total_nutrition_defecated();
        float balance = total_consumed - total_defecated;
        float total_stomach = 0.0f;
        float avg_oxygen = 0.0f;
        float avg_hunger = 0.0f;
        float avg_speed = 0.0f;
        int active_fish = 0;
        
        for (int i = 0; i < fish_count; i++) {
            if (fish_array[i].active) {
                total_stomach += fish_array[i].stomach_contents;
                avg_oxygen += fish_array[i].oxygen_level;
                avg_hunger += fish_array[i].hunger_level;
                
                Node* node = &nodes[fish_array[i].node_id];
                float speed = sqrt(node->vx * node->vx + node->vy * node->vy);
                avg_speed += speed;
                
                active_fish++;
            }
        }
        
        if (active_fish > 0) {
            avg_oxygen /= active_fish;
            avg_hunger /= active_fish;
            avg_speed /= active_fish;
        }
        
        printf("\n=== FISH SIMPLIFIED MOVEMENT (Frame %d) ===\n", current_frame);
        printf("Active fish: %d\n", active_fish);
        printf("Average speed: %.2f\n", avg_speed);
        printf("Average oxygen: %.2f\n", avg_oxygen);
        printf("Average hunger: %.2f\n", avg_hunger);
        printf("Total consumed: %.4f\n", total_consumed);
        printf("Total defecated: %.4f\n", total_defecated);
        printf("In fish stomachs: %.4f\n", total_stomach);
        printf("Fish cycle balance: %.4f\n", balance);
        printf("============================================\n\n");
    }
}