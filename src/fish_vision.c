// fish_vision.c - RL vision system for plant detection (fixed warnings)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"
#include "grid.h"
#include "plants.h"
#include "gas.h"

// Find nearest plant within FOV from fish heading
static int find_nearest_plant_in_fov(int fish_id, float* plant_vector_x, float* plant_vector_y, float* plant_distance) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        *plant_vector_x = 0.0f;
        *plant_vector_y = 0.0f;
        *plant_distance = 1.0f;  // Max normalized distance
        return 0;
    }
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) {
        *plant_vector_x = 0.0f;
        *plant_vector_y = 0.0f;
        *plant_distance = 1.0f;
        return 0;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float fish_heading = fish->heading;
    
    // Convert FOV angle to radians
    float fov_rad = (fish_type->fov_angle * M_PI) / 180.0f;
    float half_fov = fov_rad * 0.5f;
    
    float nearest_distance = 99999.0f;
    int found_plant = 0;
    float best_plant_x = 0.0f;
    float best_plant_y = 0.0f;
    
    // Get nearby cells for optimization (removed unused variable warning)
    GridCell* cells[9];
    grid_get_cells_at_position(fish_x, fish_y, cells, 9);
    
    // Search in expanding radius for performance
    float search_radius = 500.0f;  // Maximum search radius
    int node_count = simulation_get_node_count();
    
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        if (nodes[i].plant_type == -1) continue;  // Skip fish nodes
        
        float dx = nodes[i].x - fish_x;
        float dy = nodes[i].y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        // Skip if too far
        if (distance > search_radius) continue;
        
        // Calculate angle to plant relative to fish heading
        float angle_to_plant = atan2(dy, dx);
        float relative_angle = angle_to_plant - fish_heading;
        
        // Normalize angle to [-PI, PI]
        while (relative_angle > M_PI) relative_angle -= 2.0f * M_PI;
        while (relative_angle < -M_PI) relative_angle += 2.0f * M_PI;
        
        // Check if plant is within FOV
        if (fabs(relative_angle) <= half_fov) {
            if (distance < nearest_distance) {
                nearest_distance = distance;
                found_plant = 1;
                best_plant_x = nodes[i].x;
                best_plant_y = nodes[i].y;
            }
        }
    }
    
    if (found_plant) {
        // Calculate normalized vector to nearest plant
        float dx = best_plant_x - fish_x;
        float dy = best_plant_y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > 0.1f) {
            *plant_vector_x = dx / distance;
            *plant_vector_y = dy / distance;
        } else {
            *plant_vector_x = 0.0f;
            *plant_vector_y = 0.0f;
        }
        
        // Normalize distance (0.0 = very close, 1.0 = at search_radius)
        *plant_distance = fminf(distance / search_radius, 1.0f);
        
        return 1;
    } else {
        *plant_vector_x = 0.0f;
        *plant_vector_y = 0.0f;
        *plant_distance = 1.0f;  // Max distance when no plant found
        return 0;
    }
}

// Update RL inputs for fish
void fish_update_rl_inputs(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Input 0, 1 & 3: Nearest plant vector (x, y) and distance
    float plant_vector_x, plant_vector_y, plant_distance;
    find_nearest_plant_in_fov(fish_id, &plant_vector_x, &plant_vector_y, &plant_distance);
    fish->rl_inputs[0] = plant_vector_x;
    fish->rl_inputs[1] = plant_vector_y;
    fish->rl_inputs[3] = plant_distance;
    
    // Input 2: Current oxygen level at fish position
    float oxygen_level = gas_get_oxygen_at(fish_node->x, fish_node->y);
    fish->rl_inputs[2] = oxygen_level;
    
    // Debug output occasionally for all fish (not just fish 0)
    static int debug_counters[MAX_FISH] = {0};
    if (fish_id < MAX_FISH && (debug_counters[fish_id]++ % 120) == 0) {
        printf("Fish %d RL inputs: plant_vec(%.2f,%.2f), oxygen=%.2f, plant_dist=%.2f\n",
               fish_id, plant_vector_x, plant_vector_y, oxygen_level, plant_distance);
    }
}

// Get distance to nearest plant for reward calculation
float fish_get_distance_to_nearest_plant(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 99999.0f;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float nearest_distance = 99999.0f;
    
    int node_count = simulation_get_node_count();
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        if (nodes[i].plant_type == -1) continue;  // Skip fish nodes
        
        float dx = nodes[i].x - fish_x;
        float dy = nodes[i].y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance < nearest_distance) {
            nearest_distance = distance;
        }
    }
    
    return nearest_distance;
}

// Legacy function stubs for compatibility
void fish_update_vision(int fish_id) {
    // No longer used in RL system
    (void)fish_id;
}

void fish_cast_vision_ray(int fish_id, float angle, int ray_index) {
    // No longer used in RL system  
    (void)fish_id;
    (void)angle;
    (void)ray_index;
}

void fish_cast_nutrition_ray(int fish_id, float angle, int ray_index) {
    // No longer used in RL system
    (void)fish_id;
    (void)angle;
    (void)ray_index;
}

void fish_update_chemoreceptors(int fish_id) {
    // No longer used in RL system
    (void)fish_id;
}

// Legacy accessors return dummy values
float fish_get_vision_ray(int fish_id, int ray_index) {
    (void)fish_id;
    (void)ray_index;
    return 1.0f;  // No obstacles
}

float fish_get_nutrition_ray(int fish_id, int ray_index) {
    (void)fish_id;
    (void)ray_index;
    return 0.0f;  // No nutrition detected
}