#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"

static Fish* g_fish = NULL;
static FishType g_fish_types[MAX_FISH_TYPES];
static int g_fish_count = 0;
static int g_fish_type_count = 0;

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
        
        printf("Processing line: '%s'\n", line);
        
        // Skip empty lines and comments
        if (strlen(line) == 0 || line[0] == '#') {
            printf("  -> Skipped (empty/comment)\n");
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
                current_fish->node_r = 255;
                current_fish->node_g = 165;
                current_fish->node_b = 0;
                
                g_fish_type_count++;
            } else {
                printf("  -> Invalid fish name length\n");
            }
            continue;
        }
        
        if (!current_fish) {
            printf("  -> No current fish section, skipping property\n");
            continue;
        }
        
        // Parse key=value pairs
        char* equals = strchr(line, '=');
        if (!equals) {
            printf("  -> No '=' found, skipping\n");
            continue;
        }
        
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
        
        printf("  -> Property: '%s' = '%s'\n", key, value);
        
        // Parse configuration values
        if (strcmp(key, "max_speed") == 0) {
            current_fish->max_speed = (float)atof(value);
            printf("    -> Set max_speed = %.2f\n", current_fish->max_speed);
        } else if (strcmp(key, "acceleration") == 0) {
            current_fish->acceleration = (float)atof(value);
            printf("    -> Set acceleration = %.2f\n", current_fish->acceleration);
        } else if (strcmp(key, "turn_rate") == 0) {
            current_fish->turn_rate = (float)atof(value);
            printf("    -> Set turn_rate = %.2f\n", current_fish->turn_rate);
        } else if (strcmp(key, "mass") == 0) {
            current_fish->mass = (float)atof(value);
            printf("    -> Set mass = %.2f\n", current_fish->mass);
        } else if (strcmp(key, "size_radius") == 0) {
            current_fish->size_radius = (float)atof(value);
            printf("    -> Set size_radius = %.2f\n", current_fish->size_radius);
        } else if (strcmp(key, "node_color") == 0) {
            parse_color(value, &current_fish->node_r, &current_fish->node_g, &current_fish->node_b);
            printf("    -> Set color = RGB(%d, %d, %d)\n", current_fish->node_r, current_fish->node_g, current_fish->node_b);
        } else {
            printf("    -> Unknown property: '%s'\n", key);
        }
    }
    
    fclose(file);
    
    printf("Fish config loading complete!\n");
    printf("Loaded %d fish types:\n", g_fish_type_count);
    for (int i = 0; i < g_fish_type_count; i++) {
        FishType* ft = &g_fish_types[i];
        printf("  [%d] %s: speed=%.2f, mass=%.2f, radius=%.1f, color=RGB(%d,%d,%d)\n", 
               i, ft->name, ft->max_speed, ft->mass, ft->size_radius,
               ft->node_r, ft->node_g, ft->node_b);
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
    
    printf("Creating fish: type=%d, pos=(%.1f, %.1f)\n", fish_type, x, y);
    
    // Create a node in the simulation (using plant_type = -1 to mark as fish)
    int node_id = simulation_add_node(x, y, -1); // -1 indicates fish node
    if (node_id < 0) {
        printf("Error: Failed to create simulation node for fish\n");
        return -1;
    }
    
    printf("Created simulation node %d for fish\n", node_id);
    
    // Initialize fish
    Fish* fish = &g_fish[g_fish_count];
    fish->node_id = node_id;
    fish->fish_type = fish_type;
    fish->movement_force_x = 0.0f;
    fish->movement_force_y = 0.0f;
    fish->energy = 1.0f;
    fish->age = 0;
    fish->active = 1;
    
    int fish_id = g_fish_count;
    g_fish_count++;
    
    FishType* ft = &g_fish_types[fish_type];
    printf("Successfully created fish %d (type: %s) at (%.1f, %.1f) with node %d\n", 
           fish_id, ft->name, x, y, node_id);
    
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
    FishType* fish_type = &g_fish_types[fish->fish_type];
    
    // Limit force based on fish type max speed and acceleration
    float force_magnitude = sqrt(force_x * force_x + force_y * force_y);
    float max_force = fish_type->max_speed * fish_type->acceleration;
    
    if (force_magnitude > max_force && force_magnitude > 0.0f) {
        force_x = (force_x / force_magnitude) * max_force;
        force_y = (force_y / force_magnitude) * max_force;
    }
    
    fish->movement_force_x = force_x;
    fish->movement_force_y = force_y;
}

void fish_clear_movement_force(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    if (!g_fish[fish_id].active) return;
    
    g_fish[fish_id].movement_force_x = 0.0f;
    g_fish[fish_id].movement_force_y = 0.0f;
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
        
        // Simple random movement for now
        if (fish->age % 30 == 0) { // Change direction every 30 frames (0.5 seconds)
            // Random movement force
            float random_force_x = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
            float random_force_y = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
            
            // Scale by fish type speed
            random_force_x *= fish_type->max_speed * 0.3f;
            random_force_y *= fish_type->max_speed * 0.3f;
            
            // World boundary avoidance
            Node* node = &nodes[node_id];
            float boundary_distance = 1000.0f;
            
            if (node->x < WORLD_LEFT + boundary_distance) {
                random_force_x += 1.0f; // Push right
            }
            if (node->x > WORLD_RIGHT - boundary_distance) {
                random_force_x -= 1.0f; // Push left
            }
            if (node->y < WORLD_TOP + boundary_distance) {
                random_force_y += 1.0f; // Push down
            }
            if (node->y > WORLD_BOTTOM - boundary_distance) {
                random_force_y -= 1.0f; // Push up
            }
            
            fish->movement_force_x = random_force_x;
            fish->movement_force_y = random_force_y;
        }
        
        // Apply movement forces to the associated node
        float force_factor = 1.0f / fish_type->mass;
        nodes[node_id].vx += fish->movement_force_x * force_factor;
        nodes[node_id].vy += fish->movement_force_y * force_factor;
        
        // Age the fish
        fish->age++;
        
        // Energy decay (very slow for now)
        fish->energy -= 0.0001f;
        if (fish->energy < 0.0f) fish->energy = 0.0f;
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