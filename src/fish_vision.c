// fish_vision.c - Fish vision and chemoreceptor systems
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"
#include "grid.h"
#include "plants.h"

// Calculate plant nutrition cost for eating/detection
static float calculate_plant_nutrition_cost(int plant_type) {
    if (plant_type < 0 || plant_type >= plants_get_type_count()) {
        return 0.1f;
    }
    
    PlantType* pt = plants_get_type(plant_type);
    if (!pt) return 0.1f;
    
    float size_factor = (pt->max_branches / 3.0f) * (pt->branch_distance / OPTIMAL_DISTANCE);
    float actual_nutrition_cost = pt->nutrition_depletion_strength * size_factor;
    
    return actual_nutrition_cost;
}

void fish_cast_vision_ray(int fish_id, float angle, int ray_index) {
    if (fish_id < 0 || fish_id >= fish_get_count() || ray_index < 0 || ray_index >= VISION_RAYS) return;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Chain* chains = simulation_get_chains();
    
    Node* fish_node = &nodes[fish->node_id];
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    
    float ray_dx = cos(angle);
    float ray_dy = sin(angle);
    
    float min_distance = fish_type->fov_range;
    
    // Check node collisions
    int node_count = simulation_get_node_count();
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active || i == fish->node_id) continue;
        
        float node_x = nodes[i].x;
        float node_y = nodes[i].y;
        
        float to_node_x = node_x - fish_x;
        float to_node_y = node_y - fish_y;
        
        float projection = to_node_x * ray_dx + to_node_y * ray_dy;
        
        if (projection > 0 && projection < min_distance) {
            float closest_x = fish_x + ray_dx * projection;
            float closest_y = fish_y + ray_dy * projection;
            
            float dist_to_ray = sqrt((node_x - closest_x) * (node_x - closest_x) + 
                                   (node_y - closest_y) * (node_y - closest_y));
            
            float node_radius = NODE_RADIUS * 2;
            if (dist_to_ray <= node_radius) {
                float actual_distance = projection - sqrt(node_radius * node_radius - dist_to_ray * dist_to_ray);
                if (actual_distance > 0 && actual_distance < min_distance) {
                    min_distance = actual_distance;
                }
            }
        }
    }
    
    // Check chain collisions
    int chain_count = simulation_get_chain_count();
    for (int i = 0; i < chain_count; i++) {
        if (!chains[i].active) continue;
        
        int n1 = chains[i].node1;
        int n2 = chains[i].node2;
        
        if (n1 < 0 || n1 >= node_count || n2 < 0 || n2 >= node_count) continue;
        if (!nodes[n1].active || !nodes[n2].active) continue;
        
        // Line-line intersection
        float x1 = fish_x;
        float y1 = fish_y;
        float x2 = fish_x + ray_dx * fish_type->fov_range;
        float y2 = fish_y + ray_dy * fish_type->fov_range;
        
        float x3 = nodes[n1].x;
        float y3 = nodes[n1].y;
        float x4 = nodes[n2].x;
        float y4 = nodes[n2].y;
        
        float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        
        if (fabs(denom) > 0.001f) {
            float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
            float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;
            
            if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                float intersect_x = x1 + t * (x2 - x1);
                float intersect_y = y1 + t * (y2 - y1);
                
                float distance = sqrt((intersect_x - fish_x) * (intersect_x - fish_x) + 
                                    (intersect_y - fish_y) * (intersect_y - fish_y));
                
                if (distance < min_distance) {
                    min_distance = distance;
                }
            }
        }
    }
    
    fish->vision_rays[ray_index] = min_distance / fish_type->fov_range;
}

void fish_update_vision(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Use current velocity direction for ray casting
    float heading = 0.0f;
    if (fabs(fish_node->vx) > 0.01f || fabs(fish_node->vy) > 0.01f) {
        heading = atan2(fish_node->vy, fish_node->vx);
    }
    
    float half_fov = fish_type->fov_angle * 0.5f;
    for (int i = 0; i < VISION_RAYS; i++) {
        float ray_angle = heading - half_fov + (fish_type->fov_angle * i / (VISION_RAYS - 1));
        fish_cast_vision_ray(fish_id, ray_angle, i);
    }
}

void fish_cast_nutrition_ray(int fish_id, float angle, int ray_index) {
    if (fish_id < 0 || fish_id >= fish_get_count() || ray_index < 0 || ray_index >= CHEMORECEPTOR_RAYS) return;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float ray_dx = cos(angle);
    float ray_dy = sin(angle);
    
    float total_nutrition_detected = 0.0f;
    float max_range = fish_type->chemoreceptor_range;
    
    int sample_points = 10;
    for (int i = 1; i <= sample_points; i++) {
        float sample_distance = (max_range * i) / sample_points;
        float sample_x = fish_x + ray_dx * sample_distance;
        float sample_y = fish_y + ray_dy * sample_distance;
        
        GridCell* cells[9];
        int cell_count = grid_get_cells_at_position(sample_x, sample_y, cells, 9);
        
        for (int c = 0; c < cell_count; c++) {
            GridCell* cell = cells[c];
            if (!cell) continue;
            
            for (int k = 0; k < cell->count; k++) {
                int node_id = cell->node_indices[k];
                if (node_id < 0 || node_id >= simulation_get_node_count()) continue;
                if (!nodes[node_id].active) continue;
                if (nodes[node_id].plant_type == -1) continue;
                
                float dx = nodes[node_id].x - sample_x;
                float dy = nodes[node_id].y - sample_y;
                float distance_to_plant = sqrt(dx * dx + dy * dy);
                
                if (distance_to_plant <= 30.0f) {
                    float plant_nutrition = calculate_plant_nutrition_cost(nodes[node_id].plant_type);
                    
                    float distance_factor = 1.0f - (sample_distance / max_range);
                    float proximity_factor = 1.0f - (distance_to_plant / 30.0f);
                    
                    float nutrition_strength = plant_nutrition * distance_factor * proximity_factor;
                    total_nutrition_detected += nutrition_strength;
                }
            }
        }
    }
    
    fish->nutrition_rays[ray_index] = fminf(total_nutrition_detected / 0.5f, 1.0f);
}

void fish_update_chemoreceptors(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Use current velocity direction for ray casting
    float heading = 0.0f;
    if (fabs(fish_node->vx) > 0.01f || fabs(fish_node->vy) > 0.01f) {
        heading = atan2(fish_node->vy, fish_node->vx);
    }
    
    float half_fov = fish_type->fov_angle * 0.5f;
    for (int i = 0; i < CHEMORECEPTOR_RAYS; i++) {
        float ray_angle = heading - half_fov + (fish_type->fov_angle * i / (CHEMORECEPTOR_RAYS - 1));
        fish_cast_nutrition_ray(fish_id, ray_angle, i);
    }
}

// RL state accessors
float fish_get_vision_ray(int fish_id, int ray_index) {
    if (fish_id < 0 || fish_id >= fish_get_count() || ray_index < 0 || ray_index >= VISION_RAYS) return 1.0f;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 1.0f;
    
    return fish->vision_rays[ray_index];
}

float fish_get_nutrition_ray(int fish_id, int ray_index) {
    if (fish_id < 0 || fish_id >= fish_get_count() || ray_index < 0 || ray_index >= CHEMORECEPTOR_RAYS) return 0.0f;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0.0f;
    
    return fish->nutrition_rays[ray_index];
}