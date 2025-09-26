// fish_vision.c - Enhanced FOV-based vision with predator target vector system
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"
#include "grid.h"
#include "plants.h"
#include "gas.h"

// Find nearest plant within FOV for herbivores (excludes immune seeds)
static int find_nearest_plant_in_fov(int fish_id, float* plant_vector_x, float* plant_vector_y, float* plant_distance) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        *plant_vector_x = 0.0f;
        *plant_vector_y = 0.0f;
        *plant_distance = 1.0f;
        return 0;
    }
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || fish_type->is_predator) {
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
    
    float fov_rad = (fish_type->fov_angle * M_PI) / 180.0f;
    float half_fov = fov_rad * 0.5f;
    
    float nearest_distance = 99999.0f;
    int found_plant = 0;
    float best_plant_x = 0.0f;
    float best_plant_y = 0.0f;
    
    float search_radius = 1200.0f;
    int node_count = simulation_get_node_count();
    
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        if (nodes[i].plant_type < 0) continue;
        if (nodes[i].is_corpse) continue;
        
        // Skip seeds with immunity
        if (nodes[i].seed_immunity_timer > 0) {
            continue;
        }
        
        float dx = nodes[i].x - fish_x;
        float dy = nodes[i].y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > search_radius) continue;
        
        // Check FOV
        float angle_to_plant = atan2(dy, dx);
        float relative_angle = angle_to_plant - fish_heading;
        
        // Normalize angle to [-PI, PI]
        while (relative_angle > M_PI) relative_angle -= 2.0f * M_PI;
        while (relative_angle < -M_PI) relative_angle += 2.0f * M_PI;
        
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
        
        *plant_distance = fminf(distance / search_radius, 1.0f);
        return 1;
    } else {
        *plant_vector_x = 0.0f;
        *plant_vector_y = 0.0f;
        *plant_distance = 1.0f;
        return 0;
    }
}

// Find nearest prey fish within FOV for predators
static int find_nearest_prey_fish_in_fov(int fish_id, float* prey_vector_x, float* prey_vector_y, float* prey_distance) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        *prey_vector_x = 0.0f;
        *prey_vector_y = 0.0f;
        *prey_distance = 1.0f;
        return 0;
    }
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || !fish_type->is_predator) {
        *prey_vector_x = 0.0f;
        *prey_vector_y = 0.0f;
        *prey_distance = 1.0f;
        return 0;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    Fish* all_fish = fish_get_all();
    int fish_count = fish_get_count();
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float fish_heading = fish->heading;
    
    float fov_rad = (fish_type->fov_angle * M_PI) / 180.0f;
    float half_fov = fov_rad * 0.5f;
    
    float nearest_distance = 99999.0f;
    int found_prey = 0;
    float best_prey_x = 0.0f;
    float best_prey_y = 0.0f;
    
    float detection_range = fish_type->fish_detection_range;
    if (detection_range <= 0.0f) detection_range = 1000.0f;
    
    // Check living fish for prey
    for (int i = 0; i < fish_count; i++) {
        if (!all_fish[i].active) continue;
        if (i == fish_id) continue;
        
        Node* other_fish_node = &nodes[all_fish[i].node_id];
        if (!other_fish_node->active) continue;
        
        FishType* other_fish_type = fish_get_type(all_fish[i].fish_type);
        if (!other_fish_type) continue;
        
        // Skip fish of same type
        if (all_fish[i].fish_type == fish->fish_type) continue;
        
        // Only target weaker prey
        if (other_fish_type->danger_level >= fish_type->danger_level) continue;
        
        float dx = other_fish_node->x - fish_x;
        float dy = other_fish_node->y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > detection_range) continue;
        
        // FOV check
        float angle_to_fish = atan2(dy, dx);
        float relative_angle = angle_to_fish - fish_heading;
        
        while (relative_angle > M_PI) relative_angle -= 2.0f * M_PI;
        while (relative_angle < -M_PI) relative_angle += 2.0f * M_PI;
        
        if (fabs(relative_angle) <= half_fov) {
            if (distance < nearest_distance) {
                nearest_distance = distance;
                found_prey = 1;
                best_prey_x = other_fish_node->x;
                best_prey_y = other_fish_node->y;
            }
        }
    }
    
    // Check corpses for predators
    int node_count = simulation_get_node_count();
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        if (!nodes[i].is_corpse) continue;
        
        float dx = nodes[i].x - fish_x;
        float dy = nodes[i].y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > detection_range) continue;
        
        // FOV check for corpses
        float angle_to_corpse = atan2(dy, dx);
        float relative_angle = angle_to_corpse - fish_heading;
        
        while (relative_angle > M_PI) relative_angle -= 2.0f * M_PI;
        while (relative_angle < -M_PI) relative_angle += 2.0f * M_PI;
        
        if (fabs(relative_angle) <= half_fov) {
            if (distance < nearest_distance) {
                nearest_distance = distance;
                found_prey = 1;
                best_prey_x = nodes[i].x;
                best_prey_y = nodes[i].y;
            }
        }
    }
    
    if (found_prey) {
        float dx = best_prey_x - fish_x;
        float dy = best_prey_y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > 0.1f) {
            *prey_vector_x = dx / distance;
            *prey_vector_y = dy / distance;
        } else {
            *prey_vector_x = 0.0f;
            *prey_vector_y = 0.0f;
        }
        
        *prey_distance = fminf(distance / detection_range, 1.0f);
        return 1;
    } else {
        *prey_vector_x = 0.0f;
        *prey_vector_y = 0.0f;
        *prey_distance = 1.0f;
        return 0;
    }
}

// Find nearest foreign fish within FOV for threat detection
static int find_nearest_foreign_fish_in_fov(int fish_id, float* fish_vector_x, float* fish_vector_y, float* danger_level) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        *fish_vector_x = 0.0f;
        *fish_vector_y = 0.0f;
        *danger_level = 0.0f;
        return 0;
    }
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) {
        *fish_vector_x = 0.0f;
        *fish_vector_y = 0.0f;
        *danger_level = 0.0f;
        return 0;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    Fish* all_fish = fish_get_all();
    int fish_count = fish_get_count();
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float fish_heading = fish->heading;
    
    float fov_rad = (fish_type->fov_angle * M_PI) / 180.0f;
    float half_fov = fov_rad * 0.5f;
    
    float nearest_distance = 99999.0f;
    int found_target = 0;
    float best_target_x = 0.0f;
    float best_target_y = 0.0f;
    float best_danger_level = 0.0f;
    
    float detection_range = fish_type->fish_detection_range;
    if (detection_range <= 0.0f) detection_range = 800.0f;
    
    // Check living fish
    for (int i = 0; i < fish_count; i++) {
        if (!all_fish[i].active) continue;
        if (i == fish_id) continue;
        
        Node* other_fish_node = &nodes[all_fish[i].node_id];
        if (!other_fish_node->active) continue;
        
        FishType* other_fish_type = fish_get_type(all_fish[i].fish_type);
        if (!other_fish_type) continue;
        
        // Skip fish of same type
        if (all_fish[i].fish_type == fish->fish_type) continue;
        
        float dx = other_fish_node->x - fish_x;
        float dy = other_fish_node->y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > detection_range) continue;
        
        // FOV check
        float angle_to_fish = atan2(dy, dx);
        float relative_angle = angle_to_fish - fish_heading;
        
        while (relative_angle > M_PI) relative_angle -= 2.0f * M_PI;
        while (relative_angle < -M_PI) relative_angle += 2.0f * M_PI;
        
        if (fabs(relative_angle) <= half_fov) {
            // Calculate relative danger level
            float relative_danger = other_fish_type->danger_level - fish_type->danger_level;
            
            // Skip fish with same danger level (neutral)
            if (fabs(relative_danger) < 0.01f) continue;
            
            if (distance < nearest_distance) {
                nearest_distance = distance;
                found_target = 1;
                best_target_x = other_fish_node->x;
                best_target_y = other_fish_node->y;
                best_danger_level = relative_danger;
            }
        }
    }
    
    if (found_target) {
        float dx = best_target_x - fish_x;
        float dy = best_target_y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > 0.1f) {
            *fish_vector_x = dx / distance;
            *fish_vector_y = dy / distance;
        } else {
            *fish_vector_x = 0.0f;
            *fish_vector_y = 0.0f;
        }
        
        // Clamp danger level to proper range
        *danger_level = fmaxf(-1.0f, fminf(1.0f, best_danger_level));
        
        return 1;
    } else {
        *fish_vector_x = 0.0f;
        *fish_vector_y = 0.0f;
        *danger_level = 0.0f;
        return 0;
    }
}

// Enhanced RL inputs with separate predator/herbivore logic
void fish_update_rl_inputs(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Input 2: Current oxygen level (same for both types)
    float oxygen_level = gas_get_oxygen_at(fish_node->x, fish_node->y);
    fish->rl_inputs[2] = oxygen_level;
    
    if (fish_type->is_predator) {
        // Predator: Use inputs 0,1,3 for prey targeting
        float prey_vector_x, prey_vector_y, prey_distance;
        find_nearest_prey_fish_in_fov(fish_id, &prey_vector_x, &prey_vector_y, &prey_distance);
        fish->rl_inputs[0] = prey_vector_x;
        fish->rl_inputs[1] = prey_vector_y;
        fish->rl_inputs[3] = prey_distance;
        
        // Inputs 4,5,6: Threat detection (stronger predators)
        float threat_vector_x, threat_vector_y, danger_level;
        find_nearest_foreign_fish_in_fov(fish_id, &threat_vector_x, &threat_vector_y, &danger_level);
        fish->rl_inputs[4] = threat_vector_x;
        fish->rl_inputs[5] = threat_vector_y;
        fish->rl_inputs[6] = danger_level;
        
    } else {
        // Herbivore: Use inputs 0,1,3 for plant targeting
        float plant_vector_x, plant_vector_y, plant_distance;
        find_nearest_plant_in_fov(fish_id, &plant_vector_x, &plant_vector_y, &plant_distance);
        fish->rl_inputs[0] = plant_vector_x;
        fish->rl_inputs[1] = plant_vector_y;
        fish->rl_inputs[3] = plant_distance;
        
        // Inputs 4,5,6: Predator detection
        float threat_vector_x, threat_vector_y, danger_level;
        find_nearest_foreign_fish_in_fov(fish_id, &threat_vector_x, &threat_vector_y, &danger_level);
        fish->rl_inputs[4] = threat_vector_x;
        fish->rl_inputs[5] = threat_vector_y;
        fish->rl_inputs[6] = danger_level;
    }
    
    // Debug output for first few fish
    static int debug_counters[MAX_FISH] = {0};
    if (fish_id < 3 && (debug_counters[fish_id]++ % 120) == 0) {
        printf("Fish %d (%s): target_vec(%.2f,%.2f) dist=%.2f, threat_vec(%.2f,%.2f) danger=%.2f, oxygen=%.2f\n",
               fish_id, fish_type->name, 
               fish->rl_inputs[0], fish->rl_inputs[1], fish->rl_inputs[3],
               fish->rl_inputs[4], fish->rl_inputs[5], fish->rl_inputs[6], oxygen_level);
    }
}

// Get distance to nearest plant for herbivores (excludes immune seeds)
float fish_get_distance_to_nearest_plant(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 99999.0f;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type || fish_type->is_predator) return 99999.0f;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float nearest_distance = 99999.0f;
    
    int node_count = simulation_get_node_count();
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        if (nodes[i].plant_type < 0) continue;
        if (nodes[i].is_corpse) continue;
        
        // Skip seeds with immunity
        if (nodes[i].seed_immunity_timer > 0) {
            continue;
        }
        
        float dx = nodes[i].x - fish_x;
        float dy = nodes[i].y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance < nearest_distance) {
            nearest_distance = distance;
        }
    }
    
    return nearest_distance;
}

// Get distance to nearest foreign fish (FOV enforced)
float fish_get_distance_to_nearest_foreign_fish(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 99999.0f;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return 99999.0f;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    Fish* all_fish = fish_get_all();
    int fish_count = fish_get_count();
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float fish_heading = fish->heading;
    float nearest_distance = 99999.0f;
    
    float fov_rad = (fish_type->fov_angle * M_PI) / 180.0f;
    float half_fov = fov_rad * 0.5f;
    float detection_range = fish_type->fish_detection_range;
    if (detection_range <= 0.0f) detection_range = 500.0f;
    
    // Check living fish (FOV enforced)
    for (int i = 0; i < fish_count; i++) {
        if (!all_fish[i].active) continue;
        if (i == fish_id) continue;
        if (all_fish[i].fish_type == fish->fish_type) continue;
        
        Node* other_fish_node = &nodes[all_fish[i].node_id];
        if (!other_fish_node->active) continue;
        
        float dx = other_fish_node->x - fish_x;
        float dy = other_fish_node->y - fish_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > detection_range) continue;
        
        // FOV check
        float angle_to_fish = atan2(dy, dx);
        float relative_angle = angle_to_fish - fish_heading;
        
        while (relative_angle > M_PI) relative_angle -= 2.0f * M_PI;
        while (relative_angle < -M_PI) relative_angle += 2.0f * M_PI;
        
        if (fabs(relative_angle) <= half_fov) {
            if (distance < nearest_distance) {
                nearest_distance = distance;
            }
        }
    }
    
    // Check corpses (only for predators and only within FOV)
    if (fish_type->is_predator) {
        int node_count = simulation_get_node_count();
        for (int i = 0; i < node_count; i++) {
            if (!nodes[i].active) continue;
            if (!nodes[i].is_corpse) continue;
            
            float dx = nodes[i].x - fish_x;
            float dy = nodes[i].y - fish_y;
            float distance = sqrt(dx * dx + dy * dy);
            
            if (distance > detection_range) continue;
            
            // FOV check for corpses
            float angle_to_corpse = atan2(dy, dx);
            float relative_angle = angle_to_corpse - fish_heading;
            
            while (relative_angle > M_PI) relative_angle -= 2.0f * M_PI;
            while (relative_angle < -M_PI) relative_angle += 2.0f * M_PI;
            
            if (fabs(relative_angle) <= half_fov) {
                if (distance < nearest_distance) {
                    nearest_distance = distance;
                }
            }
        }
    }
    
    return nearest_distance;
}

// Legacy compatibility functions
void fish_update_vision(int fish_id) {
    (void)fish_id;
}

void fish_cast_vision_ray(int fish_id, float angle, int ray_index) {
    (void)fish_id;
    (void)angle;
    (void)ray_index;
}

void fish_cast_nutrition_ray(int fish_id, float angle, int ray_index) {
    (void)fish_id;
    (void)angle;
    (void)ray_index;
}

void fish_update_chemoreceptors(int fish_id) {
    (void)fish_id;
}

float fish_get_vision_ray(int fish_id, int ray_index) {
    (void)fish_id;
    (void)ray_index;
    return 1.0f;
}

float fish_get_nutrition_ray(int fish_id, int ray_index) {
    (void)fish_id;
    (void)ray_index;
    return 0.0f;
}