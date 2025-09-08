#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"
#include "grid.h"
#include "plants.h"

static Fish* g_fish = NULL;
static FishType g_fish_types[MAX_FISH_TYPES];
static int g_fish_count = 0;
static int g_fish_type_count = 0;
static int g_ray_rendering_enabled = 0; // Ray rendering toggle

static void parse_color(const char* color_str, int* r, int* g, int* b) {
    const char* hex = color_str;
    if (hex[0] == '#') hex++; // Skip '#' if present
    
    unsigned int color = (unsigned int)strtoul(hex, NULL, 16);
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

int fish_init(void) {
    g_fish = (Fish*)calloc(MAX_FISH, sizeof(Fish));
    
    if (!g_fish) {
        printf("Failed to allocate memory for fish\n");
        return 0;
    }
    
    g_fish_count = 0;
    g_fish_type_count = 0;
    g_ray_rendering_enabled = 0;
    
    printf("Fish system initialized\n");
    return 1;
}

void fish_cleanup(void) {
    if (g_fish) {
        free(g_fish);
        g_fish = NULL;
    }
    g_fish_count = 0;
    g_fish_type_count = 0;
}

void fish_toggle_ray_rendering(void) {
    g_ray_rendering_enabled = !g_ray_rendering_enabled;
    printf("Fish ray rendering: %s\n", g_ray_rendering_enabled ? "ON" : "OFF");
}

int fish_is_ray_rendering_enabled(void) {
    return g_ray_rendering_enabled;
}

int fish_load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open fish config file '%s'\n", filename);
        return 0;
    }
    
    printf("Loading fish config from '%s'...\n", filename);
    
    char line[256];
    FishType* current_fish = NULL;
    g_fish_type_count = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (g_fish_type_count >= MAX_FISH_TYPES) {
            printf("Warning: Maximum fish types reached (%d)\n", MAX_FISH_TYPES);
            break;
        }
        
        // Remove newline and carriage return
        line[strcspn(line, "\r\n")] = 0;
        
        // Skip empty lines and comments
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        // Check for fish type declaration [FishName]
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            current_fish = &g_fish_types[g_fish_type_count];
            memset(current_fish, 0, sizeof(FishType));
            
            // Extract fish name (remove brackets)
            int name_len = strlen(line) - 2;
            if (name_len > 0 && name_len < MAX_NAME_LENGTH) {
                strncpy(current_fish->name, line + 1, name_len);
                current_fish->name[name_len] = '\0';
                current_fish->active = 1;
                
                printf("  -> Found fish type: '%s'\n", current_fish->name);
                
                // Set default values
                current_fish->max_speed = 3.0f;
                current_fish->acceleration = 0.5f;
                current_fish->turn_rate = 0.2f;
                current_fish->mass = 1.0f;
                current_fish->size_radius = 8.0f;
                current_fish->eating_range = 80.0f;
                current_fish->eating_rate = 0.01f;
                current_fish->digestion_rate = 0.001f;
                current_fish->fov_range = 150.0f;
                current_fish->fov_angle = 1.57f; // ~90 degrees in radians
                current_fish->node_r = 255;
                current_fish->node_g = 165;
                current_fish->node_b = 0;
                
                g_fish_type_count++;
            }
            continue;
        }
        
        if (!current_fish) continue;
        
        // Parse key=value pairs
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Trim whitespace from key and value
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
        } else if (strcmp(key, "fov_range") == 0) {
            current_fish->fov_range = (float)atof(value);
        } else if (strcmp(key, "fov_angle") == 0) {
            current_fish->fov_angle = (float)atof(value);
        } else if (strcmp(key, "node_color") == 0) {
            parse_color(value, &current_fish->node_r, &current_fish->node_g, &current_fish->node_b);
        }
    }
    
    fclose(file);
    
    printf("Loaded %d fish types:\n", g_fish_type_count);
    for (int i = 0; i < g_fish_type_count; i++) {
        FishType* ft = &g_fish_types[i];
        printf("  [%d] %s: FOV_range=%.1f, FOV_angle=%.2f rad\n", 
               i, ft->name, ft->fov_range, ft->fov_angle);
    }
    
    return g_fish_type_count > 0;
}

int fish_add(float x, float y, int fish_type) {
    if (g_fish_count >= MAX_FISH) {
        printf("Error: Maximum fish count reached (%d)\n", MAX_FISH);
        return -1;
    }
    
    if (fish_type < 0 || fish_type >= g_fish_type_count) {
        printf("Error: Invalid fish type %d (available: 0-%d)\n", fish_type, g_fish_type_count - 1);
        return -1;
    }
    
    // Create a node in the simulation (using plant_type = -1 to mark as fish)
    int node_id = simulation_add_node(x, y, -1); // -1 indicates fish node
    if (node_id < 0) {
        printf("Error: Failed to create simulation node for fish\n");
        return -1;
    }
    
    // Initialize fish
    Fish* fish = &g_fish[g_fish_count];
    fish->node_id = node_id;
    fish->fish_type = fish_type;
    fish->movement_force_x = 0.0f;
    fish->movement_force_y = 0.0f;
    fish->energy = 1.0f;
    fish->stomach_contents = 0.0f;
    fish->last_eating_frame = 0;
    fish->age = 0;
    fish->active = 1;
    
    // Initialize RL state
    for (int i = 0; i < 8; i++) {
        fish->vision_rays[i] = 1.0f; // Normalized: 1.0 = max range, 0.0 = obstacle at fish
    }
    fish->hunger_level = 0.0f;
    fish->saturation_level = 1.0f;
    fish->total_reward = 0.0f;
    fish->last_reward = 0.0f;
    fish->desired_turn = 0.0f;
    fish->desired_speed = 0.5f;
    
    int fish_id = g_fish_count;
    g_fish_count++;
    
    FishType* ft = &g_fish_types[fish_type];
    printf("Created fish %d (type: %s) at (%.1f, %.1f)\n", 
           fish_id, ft->name, x, y);
    
    return fish_id;
}

void fish_remove(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    
    g_fish[fish_id].active = 0;
    printf("Removed fish %d\n", fish_id);
}

void fish_set_movement_force(int fish_id, float force_x, float force_y) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    fish->movement_force_x = force_x;
    fish->movement_force_y = force_y;
}

void fish_clear_movement_force(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    g_fish[fish_id].movement_force_x = 0.0f;
    g_fish[fish_id].movement_force_y = 0.0f;
}

void fish_cast_vision_ray(int fish_id, float angle, int ray_index) {
    if (fish_id < 0 || fish_id >= g_fish_count || ray_index < 0 || ray_index >= 8) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Chain* chains = simulation_get_chains();
    
    Node* fish_node = &nodes[fish->node_id];
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    
    // Cast ray in specified direction
    float ray_dx = cos(angle);
    float ray_dy = sin(angle);
    
    float min_distance = fish_type->fov_range;
    
    // Check collision with nodes
    int node_count = simulation_get_node_count();
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active || i == fish->node_id) continue;
        
        float node_x = nodes[i].x;
        float node_y = nodes[i].y;
        
        // Vector from fish to node
        float to_node_x = node_x - fish_x;
        float to_node_y = node_y - fish_y;
        
        // Project node onto ray direction
        float projection = to_node_x * ray_dx + to_node_y * ray_dy;
        
        if (projection > 0 && projection < min_distance) {
            // Calculate closest point on ray to node
            float closest_x = fish_x + ray_dx * projection;
            float closest_y = fish_y + ray_dy * projection;
            
            // Distance from node to ray
            float dist_to_ray = sqrt((node_x - closest_x) * (node_x - closest_x) + 
                                   (node_y - closest_y) * (node_y - closest_y));
            
            // Consider node as obstacle if ray passes close enough
            float node_radius = NODE_RADIUS * 2; // Give nodes some thickness for ray collision
            if (dist_to_ray <= node_radius) {
                float actual_distance = projection - sqrt(node_radius * node_radius - dist_to_ray * dist_to_ray);
                if (actual_distance > 0 && actual_distance < min_distance) {
                    min_distance = actual_distance;
                }
            }
        }
    }
    
    // Check collision with chains
    int chain_count = simulation_get_chain_count();
    for (int i = 0; i < chain_count; i++) {
        if (!chains[i].active) continue;
        
        int n1 = chains[i].node1;
        int n2 = chains[i].node2;
        
        if (n1 < 0 || n1 >= node_count || n2 < 0 || n2 >= node_count) continue;
        if (!nodes[n1].active || !nodes[n2].active) continue;
        
        // Simple line-line intersection check for ray vs chain
        float x1 = fish_x;
        float y1 = fish_y;
        float x2 = fish_x + ray_dx * fish_type->fov_range;
        float y2 = fish_y + ray_dy * fish_type->fov_range;
        
        float x3 = nodes[n1].x;
        float y3 = nodes[n1].y;
        float x4 = nodes[n2].x;
        float y4 = nodes[n2].y;
        
        float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        
        if (fabs(denom) > 0.001f) { // Lines not parallel
            float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
            float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;
            
            if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                // Lines intersect
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
    
    // Normalize distance (1.0 = no obstacle in range, 0.0 = obstacle at fish position)
    fish->vision_rays[ray_index] = min_distance / fish_type->fov_range;
}

void fish_update_vision(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Calculate fish heading based on velocity
    float heading = 0.0f;
    if (fabs(fish_node->vx) > 0.01f || fabs(fish_node->vy) > 0.01f) {
        heading = atan2(fish_node->vy, fish_node->vx);
    }
    
    // Cast 8 rays distributed across FOV
    float half_fov = fish_type->fov_angle * 0.5f;
    for (int i = 0; i < 8; i++) {
        float ray_angle = heading - half_fov + (fish_type->fov_angle * i / 7.0f);
        fish_cast_vision_ray(fish_id, ray_angle, i);
    }
}

void fish_calculate_hunger_saturation(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    
    // Hunger: 0.0 = full, 1.0 = starving (based on energy)
    fish->hunger_level = 1.0f - fish->energy;
    if (fish->hunger_level < 0.0f) fish->hunger_level = 0.0f;
    if (fish->hunger_level > 1.0f) fish->hunger_level = 1.0f;
    
    // Saturation: 0.0 = empty stomach, 1.0 = full stomach
    fish->saturation_level = fish->stomach_contents;
    if (fish->saturation_level < 0.0f) fish->saturation_level = 0.0f;
    if (fish->saturation_level > 1.0f) fish->saturation_level = 1.0f;
}

void fish_update_rl_state(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    fish_update_vision(fish_id);
    fish_calculate_hunger_saturation(fish_id);
}

void fish_apply_rl_action(int fish_id, float turn_action, float speed_action) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Clamp actions to valid range
    if (turn_action < -1.0f) turn_action = -1.0f;
    if (turn_action > 1.0f) turn_action = 1.0f;
    if (speed_action < 0.0f) speed_action = 0.0f;
    if (speed_action > 1.0f) speed_action = 1.0f;
    
    fish->desired_turn = turn_action;
    fish->desired_speed = speed_action;
    
    // Calculate current heading based on current velocity or default forward
    float current_heading = 0.0f;
    float current_speed = sqrt(fish_node->vx * fish_node->vx + fish_node->vy * fish_node->vy);
    
    if (current_speed > 0.1f) {
        current_heading = atan2(fish_node->vy, fish_node->vx);
    }
    
    // Apply turning - much stronger turning force
    float new_heading = current_heading + turn_action * fish_type->turn_rate * 5.0f; // 5x stronger turning
    
    // Apply speed - ensure minimum movement
    float target_speed = speed_action * fish_type->max_speed;
    if (target_speed < 0.5f && speed_action > 0.1f) {
        target_speed = 0.5f; // Minimum speed to ensure movement
    }
    
    // Calculate desired velocity
    float desired_vx = cos(new_heading) * target_speed;
    float desired_vy = sin(new_heading) * target_speed;
    
    // Apply forces directly - much stronger
    float force_strength = 2.0f; // Strong forces for responsive movement
    fish->movement_force_x = (desired_vx - fish_node->vx) * force_strength;
    fish->movement_force_y = (desired_vy - fish_node->vy) * force_strength;
    
    // Debug output occasionally
    if (fish_id == 0 && simulation_get_frame_counter() % 60 == 0) {
        printf("Fish %d: turn=%.2f, speed=%.2f, heading=%.2f, forces=(%.2f,%.2f), vel=(%.2f,%.2f)\n",
               fish_id, turn_action, speed_action, new_heading, 
               fish->movement_force_x, fish->movement_force_y,
               fish_node->vx, fish_node->vy);
    }
}

float fish_get_reward(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return 0.0f;
    if (!g_fish[fish_id].active) return 0.0f;
    
    return g_fish[fish_id].last_reward;
}

int fish_can_eat_plant(int fish_id, int node_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return 0;
    if (!g_fish[fish_id].active) return 0;
    
    Node* nodes = simulation_get_nodes();
    if (node_id < 0 || node_id >= simulation_get_node_count()) return 0;
    if (!nodes[node_id].active) return 0;
    if (nodes[node_id].plant_type == -1) return 0; // Not a plant
    
    Fish* fish = &g_fish[fish_id];
    FishType* fish_type = &g_fish_types[fish->fish_type];
    
    // Check if fish has stomach space
    if (fish->stomach_contents >= 1.0f) return 0;
    
    // Check distance
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
    
    // Check if fish can eat this frame
    if (fish->stomach_contents >= 1.0f) return; // Stomach full
    
    // Rate limiting - fish can't eat every frame
    int current_frame = simulation_get_frame_counter();
    float eating_chance = fish_type->eating_rate;
    
    // Track if fish attempted to eat
    int attempted_to_eat = 0;
    if ((float)rand() / RAND_MAX <= eating_chance) {
        attempted_to_eat = 1;
    }
    
    if (!attempted_to_eat) return;
    
    // Get fish position
    Node* fish_node = &nodes[fish->node_id];
    float fish_x = fish_node->x;
    float fish_y = fish_node->y;
    
    // Find nearby plants using spatial grid
    GridCell* cells[9];
    int cell_count = grid_get_cells_at_position(fish_x, fish_y, cells, 9);
    
    float eating_range_sq = fish_type->eating_range * fish_type->eating_range;
    int found_food = 0;
    
    for (int c = 0; c < cell_count; c++) {
        GridCell* cell = cells[c];
        if (!cell) continue;
        
        for (int k = 0; k < cell->count; k++) {
            int node_id = cell->node_indices[k];
            if (node_id < 0 || node_id >= simulation_get_node_count()) continue;
            if (!nodes[node_id].active) continue;
            if (nodes[node_id].plant_type == -1) continue; // Skip fish nodes
            
            // Check distance
            float dx = nodes[node_id].x - fish_x;
            float dy = nodes[node_id].y - fish_y;
            float distance_sq = dx * dx + dy * dy;
            
            if (distance_sq <= eating_range_sq) {
                // Found a plant in range - eat it!
                int plant_type = nodes[node_id].plant_type;
                PlantType* pt = plants_get_type(plant_type);
                
                float nutrition_gained = 0.3f; // Default nutrition
                if (pt && plant_type >= 0 && plant_type < plants_get_type_count()) {
                    nutrition_gained = pt->nutrition_value;
                }
                
                // Calculate RL reward based on nutrition gained
                float reward = nutrition_gained * 10.0f; // Scale nutrition to reward
                fish->last_reward += reward; // Add to existing reward (don't overwrite)
                fish->total_reward += reward;
                
                // Add to stomach contents
                float old_stomach = fish->stomach_contents;
                fish->stomach_contents += nutrition_gained;
                if (fish->stomach_contents > 1.0f) {
                    fish->stomach_contents = 1.0f;
                }
                
                // Additional reward for actually gaining saturation
                float saturation_gain = fish->stomach_contents - old_stomach;
                fish->last_reward += saturation_gain * 5.0f; // Bonus for saturation increase
                
                fish->last_eating_frame = current_frame;
                found_food = 1;
                
                printf("Fish %d ate plant, gained %.3f nutrition, saturation gain: %.3f, reward: %.2f\n", 
                       fish_id, nutrition_gained, saturation_gain, reward);
                
                // Remove the plant from simulation
                nodes[node_id].active = 0;
                nodes[node_id].can_grow = 0;
                
                return; // Only eat one plant per call
            }
        }
    }
    
    // Fish attempted to eat but found no food - penalty for inefficient behavior
    if (!found_food) {
        fish->last_reward -= 0.03f; // Penalty for trying to eat with no food nearby
        
        // Additional penalty if fish is in an area with no plants visible
        int plants_in_vision = 0;
        for (int i = 0; i < 8; i++) {
            if (fish->vision_rays[i] < 0.9f) { // Something detected in this ray
                plants_in_vision = 1;
                break;
            }
        }
        
        if (!plants_in_vision) {
            fish->last_reward -= 0.02f; // Extra penalty for trying to eat in empty area
        }
    }
}

// RL State accessors for Python
float fish_get_vision_ray(int fish_id, int ray_index) {
    if (fish_id < 0 || fish_id >= g_fish_count || ray_index < 0 || ray_index >= 8) return 1.0f;
    if (!g_fish[fish_id].active) return 1.0f;
    
    return g_fish[fish_id].vision_rays[ray_index];
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

void fish_update(void) {
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    
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
        
        // Get fish type for movement parameters
        FishType* fish_type = &g_fish_types[fish->fish_type];
        
        // Reset reward each frame
        fish->last_reward = 0.0f;
        
        // Calculate movement speed for this frame
        Node* node = &nodes[node_id];
        float current_speed = sqrt(node->vx * node->vx + node->vy * node->vy);
        
        // Movement reward/penalty system
        if (current_speed < 0.1f) {
            // Penalty for not moving (being stationary)
            fish->last_reward -= 0.02f; // Stronger penalty for stillness
        } else if (current_speed > 0.5f) {
            // Small reward for active movement
            fish->last_reward += 0.005f * current_speed; // Reward proportional to speed
        }
        
        // Digestion - convert stomach contents to energy
        if (fish->stomach_contents > 0.0f) {
            float digestion_amount = fish_type->digestion_rate;
            float energy_gained = digestion_amount;
            
            fish->stomach_contents -= digestion_amount;
            fish->energy += energy_gained;
            
            if (fish->stomach_contents < 0.0f) fish->stomach_contents = 0.0f;
            if (fish->energy > 1.0f) fish->energy = 1.0f;
        }
        
        // Update RL state (vision, hunger, saturation)
        fish_update_rl_state(i);
        
        // Try to eat nearby plants
        fish_eat_nearby_plants(i);
        
        // Apply movement forces to the associated node
        float force_factor = 2.0f; // Stronger force application
        nodes[node_id].vx += fish->movement_force_x * force_factor;
        nodes[node_id].vy += fish->movement_force_y * force_factor;
        
        // Ensure minimum movement if fish is trying to move
        if (fish->desired_speed > 0.1f) {
            float current_speed = sqrt(nodes[node_id].vx * nodes[node_id].vx + nodes[node_id].vy * nodes[node_id].vy);
            if (current_speed < 0.5f) {
                // Give fish a push if it's stuck
                float push_angle = (float)rand() / RAND_MAX * 2.0f * M_PI;
                nodes[node_id].vx += cos(push_angle) * 0.8f;
                nodes[node_id].vy += sin(push_angle) * 0.8f;
            }
        }
        
        // World boundary avoidance (gives negative reward for getting too close)
        float boundary_distance = 1000.0f;
        
        if (node->x < WORLD_LEFT + boundary_distance || node->x > WORLD_RIGHT - boundary_distance ||
            node->y < WORLD_TOP + boundary_distance || node->y > WORLD_BOTTOM - boundary_distance) {
            fish->last_reward -= 0.05f; // Penalty for approaching boundaries
        }
        
        // Age the fish
        fish->age++;
        
        // Energy decay (very slow)
        fish->energy -= 0.0001f;
        if (fish->energy < 0.0f) fish->energy = 0.0f;
        
        // Survival reward (small positive reward for staying alive)
        fish->last_reward += 0.001f;
    }
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