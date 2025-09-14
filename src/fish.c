// Complete and clean fish.c with simplified movement system
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"
#include "grid.h"
#include "plants.h"
#include "nutrition.h"
#include "gas.h"
#include "flow.h"

// Global fish system state
static Fish* g_fish = NULL;
static FishType g_fish_types[MAX_FISH_TYPES];
static int g_fish_count = 0;
static int g_fish_type_count = 0;
static int g_ray_rendering_enabled = 0;

// Nutrition tracking
static float g_total_nutrition_consumed = 0.0f;
static float g_total_nutrition_defecated = 0.0f;

// Utility functions
static void parse_color(const char* color_str, int* r, int* g, int* b) {
    const char* hex = color_str;
    if (hex[0] == '#') hex++; 
    
    unsigned int color = (unsigned int)strtoul(hex, NULL, 16);
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

static float calculate_plant_nutrition_cost(int plant_type) {
    if (plant_type < 0 || plant_type >= plants_get_type_count()) {
        return 0.1f;
    }
    
    PlantType* pt = plants_get_type(plant_type);
    if (!pt) return 0.1f;
    
    float size_factor = (pt->max_branches / 3.0f) * (pt->branch_distance / OPTIMAL_DISTANCE);
    return pt->nutrition_depletion_strength * size_factor;
}

// System initialization and cleanup
int fish_init(void) {
    g_fish = (Fish*)calloc(MAX_FISH, sizeof(Fish));
    
    if (!g_fish) {
        printf("Failed to allocate memory for fish\n");
        return 0;
    }
    
    g_fish_count = 0;
    g_fish_type_count = 0;
    g_ray_rendering_enabled = 0;
    g_total_nutrition_consumed = 0.0f;
    g_total_nutrition_defecated = 0.0f;
    
    printf("Fish system initialized with simplified movement (direction-only NN)\n");
    return 1;
}

void fish_cleanup(void) {
    if (g_fish) {
        free(g_fish);
        g_fish = NULL;
    }
    g_fish_count = 0;
    g_fish_type_count = 0;
    
    printf("Final nutrition stats - Consumed: %.2f, Defecated: %.2f, Balance: %.2f\n",
           g_total_nutrition_consumed, g_total_nutrition_defecated, 
           g_total_nutrition_consumed - g_total_nutrition_defecated);
}

// Ray rendering toggle
void fish_toggle_ray_rendering(void) {
    g_ray_rendering_enabled = !g_ray_rendering_enabled;
    printf("Fish ray rendering: %s\n", g_ray_rendering_enabled ? "ON" : "OFF");
}

int fish_is_ray_rendering_enabled(void) {
    return g_ray_rendering_enabled;
}

// Fish type configuration loading
int fish_load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open fish config file '%s'\n", filename);
        return 0;
    }
    
    printf("Loading simplified movement fish config from '%s'...\n", filename);
    
    char line[256];
    FishType* current_fish = NULL;
    g_fish_type_count = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (g_fish_type_count >= MAX_FISH_TYPES) {
            printf("Warning: Maximum fish types reached (%d)\n", MAX_FISH_TYPES);
            break;
        }
        
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            current_fish = &g_fish_types[g_fish_type_count];
            memset(current_fish, 0, sizeof(FishType));
            
            int name_len = strlen(line) - 2;
            if (name_len > 0 && name_len < MAX_NAME_LENGTH) {
                strncpy(current_fish->name, line + 1, name_len);
                current_fish->name[name_len] = '\0';
                current_fish->active = 1;
                
                // Set default values
                current_fish->max_speed = 3.0f;
                current_fish->acceleration = 1.0f;
                current_fish->turn_rate = 0.2f;
                current_fish->mass = 1.0f;
                current_fish->size_radius = 8.0f;
                current_fish->eating_range = 80.0f;
                current_fish->eating_rate = 0.01f;
                current_fish->digestion_rate = 0.001f;
                current_fish->defecation_rate = 0.006f;
                current_fish->defecation_radius = 60.0f;
                current_fish->fov_range = 200.0f;
                current_fish->fov_angle = 3.14159f;
                current_fish->chemoreceptor_range = 150.0f;
                current_fish->oxygen_consumption_rate = 0.0004f;
                current_fish->oxygen_refill_rate = 0.003f;
                current_fish->flow_sensitivity = 0.5f;
                current_fish->node_r = 255;
                current_fish->node_g = 165;
                current_fish->node_b = 0;
                
                g_fish_type_count++;
            }
            continue;
        }
        
        if (!current_fish) continue;
        
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Trim whitespace
        while (*key == ' ' || *key == '\t') key++;
        char* key_end = key + strlen(key) - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
            *key_end = '\0';
            key_end--;
        }
        
        while (*value == ' ' || *value == '\t') value++;
        char* value_end = value + strlen(value) - 1;
        while (value_end > value && (*value_end == ' ' || *value_end == '\t')) {
            *value_end = '\0';
            value_end--;
        }
        
        // Parse configuration values
        if (strcmp(key, "max_speed") == 0) {
            current_fish->max_speed = (float)atof(value);
        } else if (strcmp(key, "acceleration") == 0) {
            current_fish->acceleration = (float)atof(value);
        } else if (strcmp(key, "turn_rate") == 0) {
            current_fish->turn_rate = (float)atof(value);
        } else if (strcmp(key, "mass") == 0) {
            current_fish->mass = (float)atof(value);
        } else if (strcmp(key, "size_radius") == 0) {
            current_fish->size_radius = (float)atof(value);
        } else if (strcmp(key, "eating_range") == 0) {
            current_fish->eating_range = (float)atof(value);
        } else if (strcmp(key, "eating_rate") == 0) {
            current_fish->eating_rate = (float)atof(value);
        } else if (strcmp(key, "digestion_rate") == 0) {
            current_fish->digestion_rate = (float)atof(value);
        } else if (strcmp(key, "defecation_rate") == 0) {
            current_fish->defecation_rate = (float)atof(value);
        } else if (strcmp(key, "defecation_radius") == 0) {
            current_fish->defecation_radius = (float)atof(value);
        } else if (strcmp(key, "fov_range") == 0) {
            current_fish->fov_range = (float)atof(value);
        } else if (strcmp(key, "fov_angle") == 0) {
            current_fish->fov_angle = (float)atof(value);
        } else if (strcmp(key, "chemoreceptor_range") == 0) {
            current_fish->chemoreceptor_range = (float)atof(value);
        } else if (strcmp(key, "oxygen_consumption_rate") == 0) {
            current_fish->oxygen_consumption_rate = (float)atof(value);
        } else if (strcmp(key, "oxygen_refill_rate") == 0) {
            current_fish->oxygen_refill_rate = (float)atof(value);
        } else if (strcmp(key, "flow_sensitivity") == 0) {
            current_fish->flow_sensitivity = (float)atof(value);
        } else if (strcmp(key, "node_color") == 0) {
            parse_color(value, &current_fish->node_r, &current_fish->node_g, &current_fish->node_b);
        }
    }
    
    fclose(file);
    
    printf("Loaded %d fish types with simplified movement system\n", g_fish_type_count);
    return g_fish_type_count > 0;
}

// Fish management functions
int fish_add(float x, float y, int fish_type) {
    if (g_fish_count >= MAX_FISH) {
        printf("Error: Maximum fish count reached (%d)\n", MAX_FISH);
        return -1;
    }
    
    if (fish_type < 0 || fish_type >= g_fish_type_count) {
        printf("Error: Invalid fish type %d (available: 0-%d)\n", fish_type, g_fish_type_count - 1);
        return -1;
    }
    
    int node_id = simulation_add_node(x, y, -1);
    if (node_id < 0) {
        printf("Error: Failed to create simulation node for fish\n");
        return -1;
    }
    
    Fish* fish = &g_fish[g_fish_count];
    fish->node_id = node_id;
    fish->fish_type = fish_type;
    
    // FIXED: Initialize with ZERO movement to prevent instant rushing
    fish->desired_direction_x = 0.0f;
    fish->desired_direction_y = 0.0f;
    fish->movement_force_x = 0.0f;
    fish->movement_force_y = 0.0f;
    
    // Initialize fish state
    fish->energy = 1.0f;
    fish->stomach_contents = 0.0f;
    fish->consumed_nutrition = 0.0f;
    fish->last_eating_frame = 0;
    fish->last_defecation_frame = 0;
    fish->age = 0;
    fish->active = 1;
    
    // Initialize RL state
    fish->oxygen_level = 1.0f;
    fish->hunger_level = 0.0f;
    for (int i = 0; i < VISION_RAYS; i++) {
        fish->vision_rays[i] = 1.0f;
    }
    for (int i = 0; i < CHEMORECEPTOR_RAYS; i++) {
        fish->nutrition_rays[i] = 0.0f;
    }
    fish->saturation_level = 1.0f;
    fish->total_reward = 0.0f;
    fish->last_reward = 0.0f;
    
    int fish_id = g_fish_count;
    g_fish_count++;
    
    // ADDED: Ensure the simulation node also starts with zero velocity
    Node* nodes = simulation_get_nodes();
    nodes[node_id].vx = 0.0f;
    nodes[node_id].vy = 0.0f;
    
    FishType* ft = &g_fish_types[fish_type];
    printf("Created stationary fish %d (%s) at (%.1f, %.1f)\n", 
           fish_id, ft->name, x, y);
    
    return fish_id;
}

void fish_remove(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    
    g_fish[fish_id].active = 0;
    printf("Removed fish %d\n", fish_id);
}

// Movement control functions
void fish_set_movement_direction(int fish_id, float direction_x, float direction_y) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    
    // Clamp to valid range
    fish->desired_direction_x = fmaxf(-1.0f, fminf(1.0f, direction_x));
    fish->desired_direction_y = fmaxf(-1.0f, fminf(1.0f, direction_y));
}

void fish_clear_movement_force(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    g_fish[fish_id].desired_direction_x = 0.0f;
    g_fish[fish_id].desired_direction_y = 0.0f;
    g_fish[fish_id].movement_force_x = 0.0f;
    g_fish[fish_id].movement_force_y = 0.0f;
}

// RL system functions
void fish_update_oxygen_system(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float environmental_oxygen = gas_get_oxygen_at(fish_node->x, fish_node->y);
    
    // Oxygen consumption
    fish->oxygen_level -= fish_type->oxygen_consumption_rate * 0.5f;
    
    // Oxygen refill in rich environments
    if (environmental_oxygen > 0.1f) {
        float refill_rate = fish_type->oxygen_refill_rate * 2.0f * (environmental_oxygen + 0.2f);
        fish->oxygen_level += refill_rate;
    }
    
    // Clamp oxygen level
    fish->oxygen_level = fmaxf(0.0f, fminf(1.0f, fish->oxygen_level));
}

void fish_update_hunger_system(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    
    // Gradual hunger increase
    fish->hunger_level += 0.0003f;
    
    // Clamp hunger level
    fish->hunger_level = fmaxf(0.0f, fminf(1.0f, fish->hunger_level));
    
    // Update saturation based on stomach contents
    fish->saturation_level = fminf(1.0f, fish->stomach_contents);
}

// Vision system functions
void fish_cast_vision_ray(int fish_id, float angle, int ray_index) {
    if (fish_id < 0 || fish_id >= g_fish_count || ray_index < 0 || ray_index >= VISION_RAYS) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
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
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float heading = 0.0f;
    if (fabs(fish_node->vx) > 0.01f || fabs(fish_node->vy) > 0.01f) {
        heading = atan2(fish_node->vy, fish_node->vx);
    }
    
    // Cast vision rays across 180° field of view
    float half_fov = fish_type->fov_angle * 0.5f;
    for (int i = 0; i < VISION_RAYS; i++) {
        float ray_angle = heading - half_fov + (fish_type->fov_angle * i / (VISION_RAYS - 1));
        fish_cast_vision_ray(fish_id, ray_angle, i);
    }
}

// Chemoreceptor system functions
void fish_cast_nutrition_ray(int fish_id, float angle, int ray_index) {
    if (fish_id < 0 || fish_id >= g_fish_count || ray_index < 0 || ray_index >= CHEMORECEPTOR_RAYS) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    float ray_dx = cos(angle);
    float ray_dy = sin(angle);
    
    float total_nutrition_detected = 0.0f;
    float max_range = fish_type->chemoreceptor_range;
    
    // Sample nutrition along the ray
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
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float heading = 0.0f;
    if (fabs(fish_node->vx) > 0.01f || fabs(fish_node->vy) > 0.01f) {
        heading = atan2(fish_node->vy, fish_node->vx);
    }
    
    // Cast chemoreceptor rays
    float half_fov = fish_type->fov_angle * 0.5f;
    for (int i = 0; i < CHEMORECEPTOR_RAYS; i++) {
        float ray_angle = heading - half_fov + (fish_type->fov_angle * i / (CHEMORECEPTOR_RAYS - 1));
        fish_cast_nutrition_ray(fish_id, ray_angle, i);
    }
}

void fish_update_rl_state(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    fish_update_vision(fish_id);
    fish_update_chemoreceptors(fish_id);
    fish_update_oxygen_system(fish_id);
    fish_update_hunger_system(fish_id);
}

// Reward calculation functions
void fish_calculate_chemoreceptor_rewards(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    
    float total_nutrition_detected = 0.0f;
    float max_ray_nutrition = 0.0f;
    
    for (int i = 0; i < CHEMORECEPTOR_RAYS; i++) {
        total_nutrition_detected += fish->nutrition_rays[i];
        if (fish->nutrition_rays[i] > max_ray_nutrition) {
            max_ray_nutrition = fish->nutrition_rays[i];
        }
    }
    
    if (total_nutrition_detected > 0.1f) {
        float detection_reward = total_nutrition_detected * 0.02f;
        fish->last_reward += detection_reward;
        
        if (max_ray_nutrition > 0.5f) {
            fish->last_reward += 0.01f;
        }
        
        if (fish->hunger_level > 0.5f) {
            fish->last_reward += total_nutrition_detected * fish->hunger_level * 0.01f;
        }
    }
}

void fish_calculate_oxygen_rewards(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    float environmental_oxygen = gas_get_oxygen_at(fish_node->x, fish_node->y);
    
    // Reward for being in oxygen-rich areas
    if (environmental_oxygen > 0.3f) {
        float oxygen_richness = (environmental_oxygen - 0.3f) / 0.7f;
        float oxygen_reward = oxygen_richness * oxygen_richness * 0.015f;
        fish->last_reward += oxygen_reward;
        
        if (environmental_oxygen > 0.8f) {
            fish->last_reward += 0.008f;
        }
    }
    
    // Small penalty for very low oxygen areas
    if (environmental_oxygen < 0.1f) {
        fish->last_reward -= 0.005f;
    }
}

void fish_calculate_environmental_rewards(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Reduced penalties for low oxygen/high hunger
    if (fish->oxygen_level < 0.3f) {
        float oxygen_deficit = 0.3f - fish->oxygen_level;
        fish->last_reward -= oxygen_deficit * oxygen_deficit * 0.02f;
    }
    
    if (fish->hunger_level > 0.8f) {
        fish->last_reward -= (fish->hunger_level - 0.8f) * 0.01f;
    }
    
    // Movement reward - encourage active swimming
    float current_speed = sqrt(fish_node->vx * fish_node->vx + fish_node->vy * fish_node->vy);
    if (current_speed > 0.2f) {
        float movement_reward = current_speed * 0.015f;
        if (current_speed > 1.5f) {
            movement_reward += 0.01f;
        }
        fish->last_reward += movement_reward;
    } else {
        // Small penalty for being stationary
        fish->last_reward -= 0.003f;
    }
    
    // Bonus for excellent condition
    if (fish->oxygen_level > 0.7f && fish->hunger_level < 0.4f) {
        fish->last_reward += 0.008f;
    }
}

// Movement calculation
static void calculate_fish_movement(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Calculate desired velocity from direction
    float direction_magnitude = sqrt(fish->desired_direction_x * fish->desired_direction_x + 
                                   fish->desired_direction_y * fish->desired_direction_y);
    
    float desired_vx = 0.0f;
    float desired_vy = 0.0f;
    
    if (direction_magnitude > 0.1f) {
        // Normalize direction and apply MUCH SMALLER max speed
        float normalized_x = fish->desired_direction_x / direction_magnitude;
        float normalized_y = fish->desired_direction_y / direction_magnitude;
        
        // FIXED: Much slower max speed
        float actual_max_speed = fish_type->max_speed * 0.3f;  // Only 30% of config speed
        desired_vx = normalized_x * actual_max_speed;
        desired_vy = normalized_y * actual_max_speed;
    }
    
    // FIXED: Much gentler force application
    float force_strength = fish_type->acceleration * 0.5f;  // REDUCED: was * 10.0f
    fish->movement_force_x = (desired_vx - fish_node->vx) * force_strength;
    fish->movement_force_y = (desired_vy - fish_node->vy) * force_strength;
    
    // ADDED: Clamp forces to prevent extreme acceleration
    float max_force = fish_type->max_speed * 0.1f;
    if (fish->movement_force_x > max_force) fish->movement_force_x = max_force;
    if (fish->movement_force_x < -max_force) fish->movement_force_x = -max_force;
    if (fish->movement_force_y > max_force) fish->movement_force_y = max_force;
    if (fish->movement_force_y < -max_force) fish->movement_force_y = -max_force;
}

// RL action application
void fish_apply_rl_action(int fish_id, float direction_x, float direction_y) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    fish_set_movement_direction(fish_id, direction_x, direction_y);
}

// Eating and nutrition functions
int fish_can_eat_plant(int fish_id, int node_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return 0;
    if (!g_fish[fish_id].active) return 0;
    
    Node* nodes = simulation_get_nodes();
    if (node_id < 0 || node_id >= simulation_get_node_count()) return 0;
    if (!nodes[node_id].active) return 0;
    if (nodes[node_id].plant_type == -1) return 0;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    
    Node* fish_node = &nodes[fish->node_id];
    Node* plant_node = &nodes[node_id];
    
    float dx = plant_node->x - fish_node->x;
    float dy = plant_node->y - fish_node->y;
    float distance = sqrt(dx * dx + dy * dy);
    
    return distance <= fish_type->eating_range;
}

void fish_eat_nearby_plants(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    
    int current_frame = simulation_get_frame_counter();
    float eating_chance = fish_type->eating_rate;
    
    if ((float)rand() / RAND_MAX > eating_chance) {
        return;
    }
    
    Node* fish_node = &nodes[fish->node_id];
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    
    GridCell* cells[9];
    int cell_count = grid_get_cells_at_position(fish_x, fish_y, cells, 9);
    
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    
    for (int c = 0; c < cell_count; c++) {
        GridCell* cell = cells[c];
        if (!cell) continue;
        
        for (int k = 0; k < cell->count; k++) {
            int node_id = cell->node_indices[k];
            if (node_id < 0 || node_id >= simulation_get_node_count()) continue;
            if (!nodes[node_id].active) continue;
            if (nodes[node_id].plant_type == -1) continue;
            
            float dx = nodes[node_id].x - fish_x;
            float dy = nodes[node_id].y - fish_y;
            float distance_sq = dx * dx + dy * dy;
            
            if (distance_sq <= eating_range_sq) {
                int plant_type = nodes[node_id].plant_type;
                PlantType* pt = plants_get_type(plant_type);
                if (!pt) continue;
                
                float plant_nutrition = calculate_plant_nutrition_cost(plant_type);
                
                printf("FISH EAT: Fish %d eating %s (nutrition: %.3f)\n", 
                       fish_id, pt->name, plant_nutrition);
                
                // Add to stomach and reduce hunger
                fish->stomach_contents += plant_nutrition;
                fish->hunger_level -= plant_nutrition * 0.8f;
                if (fish->hunger_level < 0.0f) fish->hunger_level = 0.0f;
                
                g_total_nutrition_consumed += plant_nutrition;
                
                // Calculate rewards
                float base_nutrition_reward = plant_nutrition * 25.0f;
                
                float nutrition_bonus = 0.0f;
                if (plant_nutrition > 0.4f) {
                    nutrition_bonus = (plant_nutrition - 0.4f) * 30.0f;
                } else if (plant_nutrition < 0.2f) {
                    nutrition_bonus = -0.03f;
                }
                
                float hunger_bonus = fish->hunger_level * 0.2f;
                
                float total_reward = base_nutrition_reward + nutrition_bonus + hunger_bonus;
                fish->last_reward += total_reward;
                fish->total_reward += total_reward;
                
                fish->last_eating_frame = current_frame;
                
                // Remove eaten plant
                nodes[node_id].active = 0;
                nodes[node_id].can_grow = 0;
                
                return; // Only eat one plant per frame
            }
        }
    }
    
    // Small penalty for not finding food when very hungry
    if (fish->hunger_level > 0.9f) {
        fish->last_reward -= 0.005f;
    }
}

void fish_defecate(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    if (fish->stomach_contents < 0.7f) {
        return;
    }
    
    float defecation_amount = fish->stomach_contents;
    
    printf("FISH DEFECATE: Fish %d defecating %.4f at position (%.1f, %.1f)\n", 
           fish_id, defecation_amount, fish_node->x, fish_node->y);
    
    nutrition_add_at_position(fish_node->x, fish_node->y, 
                              defecation_amount, fish_type->defecation_radius);
    
    fish->stomach_contents = 0.0f;
    g_total_nutrition_defecated += defecation_amount;
    
    // 30% chance to spawn a random seed
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
                    PlantType* pt = plants_get_type(random_plant_type);
                    printf("FISH SEED: Fish %d planted %s seed\n", fish_id, pt->name);
                }
            }
        }
    }
}

// Accessor functions
float fish_get_vision_ray(int fish_id, int ray_index) {
    if (fish_id < 0 || fish_id >= g_fish_count || ray_index < 0 || ray_index >= VISION_RAYS) return 1.0f;
    if (!g_fish[fish_id].active) return 1.0f;
    
    return g_fish[fish_id].vision_rays[ray_index];
}

float fish_get_nutrition_ray(int fish_id, int ray_index) {
    if (fish_id < 0 || fish_id >= g_fish_count || ray_index < 0 || ray_index >= CHEMORECEPTOR_RAYS) return 0.0f;
    if (!g_fish[fish_id].active) return 0.0f;
    
    return g_fish[fish_id].nutrition_rays[ray_index];
}

float fish_get_oxygen_level(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return 0.0f;
    if (!g_fish[fish_id].active) return 0.0f;
    
    return g_fish[fish_id].oxygen_level;
}

float fish_get_hunger_level(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return 0.0f;
    if (!g_fish[fish_id].active) return 0.0f;
    
    return g_fish[fish_id].hunger_level;
}

float fish_get_saturation_level(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return 0.0f;
    if (!g_fish[fish_id].active) return 0.0f;
    
    return g_fish[fish_id].saturation_level;
}

float fish_get_last_reward(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return 0.0f;
    if (!g_fish[fish_id].active) return 0.0f;
    
    return g_fish[fish_id].last_reward;
}

float fish_get_reward(int fish_id) {
    return fish_get_last_reward(fish_id);
}

int fish_get_count(void) {
    return g_fish_count;
}

Fish* fish_get_by_id(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return NULL;
    if (!g_fish[fish_id].active) return NULL;
    return &g_fish[fish_id];
}

Fish* fish_get_all(void) {
    return g_fish;
}

int fish_get_type_count(void) {
    return g_fish_type_count;
}

FishType* fish_get_type(int index) {
    if (index < 0 || index >= g_fish_type_count) {
        return NULL;
    }
    return &g_fish_types[index];
}

float fish_get_total_nutrition_consumed(void) {
    return g_total_nutrition_consumed;
}

float fish_get_total_nutrition_defecated(void) {
    return g_total_nutrition_defecated;
}

float fish_get_nutrition_balance(void) {
    return g_total_nutrition_consumed - g_total_nutrition_defecated;
}

// Main fish update function
void fish_update(void) {
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    int current_frame = simulation_get_frame_counter();
    
    for (int i = 0; i < g_fish_count; i++) {
        if (!g_fish[i].active) continue;
        
        Fish* fish = &g_fish[i];
        int node_id = fish->node_id;
        
        if (node_id < 0 || node_id >= node_count) {
            printf("Warning: Fish %d has invalid node_id %d\n", i, node_id);
            continue;
        }
        if (!nodes[node_id].active) {
            printf("Warning: Fish %d node %d is not active\n", i, node_id);
            continue;
        }
        
        FishType* fish_type = &g_fish_types[fish->fish_type];
        
        // Reset frame rewards
        fish->last_reward = 0.0f;
        
        Node* node = &nodes[node_id];
        
        // ADDED: Hard boundary enforcement - teleport back if outside
        if (node->x < WORLD_LEFT) {
            node->x = WORLD_LEFT + 10.0f;
            node->vx = 0.0f;
            printf("Fish %d teleported back from left boundary\n", i);
        }
        if (node->x > WORLD_RIGHT) {
            node->x = WORLD_RIGHT - 10.0f;
            node->vx = 0.0f;
            printf("Fish %d teleported back from right boundary\n", i);
        }
        if (node->y < WORLD_TOP) {
            node->y = WORLD_TOP + 10.0f;
            node->vy = 0.0f;
            printf("Fish %d teleported back from top boundary\n", i);
        }
        if (node->y > WORLD_BOTTOM) {
            node->y = WORLD_BOTTOM - 10.0f;
            node->vy = 0.0f;
            printf("Fish %d teleported back from bottom boundary\n", i);
        }
        
        // ADDED: Speed limiting - prevent excessive speeds
        float current_speed = sqrt(node->vx * node->vx + node->vy * node->vy);
        float max_allowed_speed = fish_type->max_speed * 0.4f;  // 40% of config speed
        
        if (current_speed > max_allowed_speed) {
            float speed_factor = max_allowed_speed / current_speed;
            node->vx *= speed_factor;
            node->vy *= speed_factor;
            printf("Fish %d speed limited from %.2f to %.2f\n", i, current_speed, max_allowed_speed);
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
        
        // Update RL state and calculate rewards
        fish_update_rl_state(i);
        fish_calculate_environmental_rewards(i);
        fish_calculate_chemoreceptor_rewards(i);
        fish_calculate_oxygen_rewards(i);
        
        // Try to eat and defecate
        fish_eat_nearby_plants(i);
        fish_defecate(i);
        
        // Calculate and apply movement forces
        calculate_fish_movement(i);
        
        // FIXED: Much gentler force application
        float force_factor = 0.3f;  // REDUCED: was 1.0f
        nodes[node_id].vx += fish->movement_force_x * force_factor;
        nodes[node_id].vy += fish->movement_force_y * force_factor;
        
        // Apply flow field influence (keep this gentle)
        float flow_x, flow_y;
        flow_get_vector_at(node->x, node->y, &flow_x, &flow_y);
        
        float flow_influence = fish_type->flow_sensitivity * 0.5f;  // REDUCED: was 1.0f
        nodes[node_id].vx += flow_x * flow_influence;
        nodes[node_id].vy += flow_y * flow_influence;
        
        // Boundary penalties (softer)
        float boundary_distance = 1000.0f;
        if (node->x < WORLD_LEFT + boundary_distance || node->x > WORLD_RIGHT - boundary_distance ||
            node->y < WORLD_TOP + boundary_distance || node->y > WORLD_BOTTOM - boundary_distance) {
            fish->last_reward -= 0.005f;  // REDUCED: was 0.01f
        }
        
        // Age and energy decay
        fish->age++;
        fish->energy -= 0.00001f;  // REDUCED: was 0.00002f
        if (fish->energy < 0.0f) fish->energy = 0.0f;
        
        // Base survival reward
        fish->last_reward += 0.005f;  // REDUCED: was 0.008f
    }
    
    // Debug output every 30 seconds (longer interval)
    static int last_debug_frame = 0;
    if (current_frame - last_debug_frame >= 1800) {  // INCREASED: was 1200
        last_debug_frame = current_frame;
        
        float balance = g_total_nutrition_consumed - g_total_nutrition_defecated;
        float total_stomach = 0.0f;
        float avg_oxygen = 0.0f;
        float avg_hunger = 0.0f;
        float avg_speed = 0.0f;
        int active_fish = 0;
        int out_of_bounds = 0;
        
        for (int i = 0; i < g_fish_count; i++) {
            if (g_fish[i].active) {
                Node* node = &nodes[g_fish[i].node_id];
                float speed = sqrt(node->vx * node->vx + node->vy * node->vy);
                
                // Check if fish is near boundaries
                if (node->x < WORLD_LEFT + 1000 || node->x > WORLD_RIGHT - 1000 ||
                    node->y < WORLD_TOP + 1000 || node->y > WORLD_BOTTOM - 1000) {
                    out_of_bounds++;
                }
                
                total_stomach += g_fish[i].stomach_contents;
                avg_oxygen += g_fish[i].oxygen_level;
                avg_hunger += g_fish[i].hunger_level;
                avg_speed += speed;
                
                active_fish++;
            }
        }
        
        if (active_fish > 0) {
            avg_oxygen /= active_fish;
            avg_hunger /= active_fish;
            avg_speed /= active_fish;
        }
        
        printf("\n=== FIXED FISH MOVEMENT SYSTEM (Frame %d) ===\n", current_frame);
        printf("Active fish: %d (near boundaries: %d)\n", active_fish, out_of_bounds);
        printf("Average speed: %.2f (controlled movement)\n", avg_speed);
        printf("Average oxygen level: %.2f\n", avg_oxygen);
        printf("Average hunger level: %.2f\n", avg_hunger);
        printf("Total consumed: %.4f\n", g_total_nutrition_consumed);
        printf("Total defecated: %.4f\n", g_total_nutrition_defecated);
        printf("Fish cycle balance: %.4f\n", balance);
        printf("System: Fixed speeds, boundary enforcement, gentle movement\n");
        printf("================================================\n\n");
    }
}