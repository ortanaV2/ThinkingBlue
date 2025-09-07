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
static int g_player_fish = -1;
static int g_player_keys[4] = {0};

static void parse_color(const char* color_str, int* r, int* g, int* b) {
    const char* hex = color_str;
    if (hex[0] == '#') hex++; // Skip '#' if present
    
    unsigned int color = (unsigned int)strtoul(hex, NULL, 16);
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

int fish_load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Warning: Cannot open fish config file '%s', using defaults\n", filename);
        
        // Create default fish type
        FishType* default_fish = &g_fish_types[0];
        strcpy(default_fish->name, "DefaultFish");
        default_fish->acceleration = 1.5f;
        default_fish->eat_radius = FISH_EAT_RADIUS;
        default_fish->hunger_rate = 0.001f;
        default_fish->fish_r = 100;
        default_fish->fish_g = 150;
        default_fish->fish_b = 255;
        default_fish->active = 1;
        g_fish_type_count = 1;
        
        return 1;
    }
    
    char line[256];
    FishType* current_fish = NULL;
    g_fish_type_count = 0;
    
    while (fgets(line, sizeof(line), file) && g_fish_type_count < MAX_FISH_TYPES) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;
        
        // Check for fish type declaration [FishName]
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            current_fish = &g_fish_types[g_fish_type_count];
            memset(current_fish, 0, sizeof(FishType));
            
            // Extract fish name
            strncpy(current_fish->name, line + 1, strlen(line) - 2);
            current_fish->name[strlen(line) - 2] = '\0';
            current_fish->active = 1;
            
            // Set default values
            current_fish->acceleration = 1.5f;
            current_fish->eat_radius = FISH_EAT_RADIUS;
            current_fish->hunger_rate = 0.001f;
            current_fish->fish_r = 100;
            current_fish->fish_g = 150;
            current_fish->fish_b = 255;
            
            g_fish_type_count++;
            continue;
        }
        
        if (!current_fish) continue;
        
        // Parse key=value pairs
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Trim whitespace
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        // Parse configuration values
        if (strcmp(key, "acceleration") == 0) {
            current_fish->acceleration = (float)atof(value);
        } else if (strcmp(key, "eat_radius") == 0) {
            current_fish->eat_radius = (float)atof(value);
        } else if (strcmp(key, "hunger_rate") == 0) {
            current_fish->hunger_rate = (float)atof(value);
        } else if (strcmp(key, "fish_color") == 0) {
            parse_color(value, &current_fish->fish_r, &current_fish->fish_g, &current_fish->fish_b);
        }
    }
    
    fclose(file);
    
    if (g_fish_type_count == 0) {
        // Fallback to default if no types loaded
        FishType* default_fish = &g_fish_types[0];
        strcpy(default_fish->name, "DefaultFish");
        default_fish->acceleration = 1.5f;
        default_fish->eat_radius = FISH_EAT_RADIUS;
        default_fish->hunger_rate = 0.001f;
        default_fish->fish_r = 100;
        default_fish->fish_g = 150;
        default_fish->fish_b = 255;
        default_fish->active = 1;
        g_fish_type_count = 1;
    }
    
    printf("Loaded %d fish types from config\n", g_fish_type_count);
    return g_fish_type_count > 0;
}

int fish_init(void) {
    g_fish = (Fish*)calloc(MAX_FISH, sizeof(Fish));
    if (!g_fish) {
        printf("Failed to allocate memory for fish\n");
        return 0;
    }
    
    g_fish_count = 0;
    g_player_fish = -1;
    
    // Load fish configuration
    fish_load_config("fish.conf");
    
    printf("Fish system initialized\n");
    return 1;
}

void fish_cleanup(void) {
    if (g_fish) {
        free(g_fish);
        g_fish = NULL;
    }
    g_fish_count = 0;
    g_player_fish = -1;
}

int fish_add(float x, float y, int fish_type) {
    if (g_fish_count >= MAX_FISH) {
        printf("Maximum fish reached\n");
        return -1;
    }
    
    // Clamp to world bounds
    if (x < WORLD_LEFT) x = WORLD_LEFT;
    if (x > WORLD_RIGHT) x = WORLD_RIGHT;
    if (y < WORLD_TOP) y = WORLD_TOP;
    if (y > WORLD_BOTTOM) y = WORLD_BOTTOM;
    
    Fish* fish = &g_fish[g_fish_count];
    fish->x = x;
    fish->y = y;
    fish->vx = 0;
    fish->vy = 0;
    fish->active = 1;
    fish->fish_type = fish_type;
    fish->hunger = 0.5f;
    fish->age = 0;
    fish->is_player = 0;
    fish->force_x = 0;
    fish->force_y = 0;
    
    // First fish becomes player controlled
    if (g_player_fish == -1) {
        g_player_fish = g_fish_count;
        fish->is_player = 1;
        printf("Fish %d is now player controlled\n", g_fish_count);
    }
    
    return g_fish_count++;
}

void fish_set_player_control(int keys[4]) {
    for (int i = 0; i < 4; i++) {
        g_player_keys[i] = keys[i];
    }
}

void fish_player_eat(void) {
    if (g_player_fish < 0 || g_player_fish >= g_fish_count) return;
    if (!g_fish[g_player_fish].active) return;
    
    Fish* player = &g_fish[g_player_fish];
    FishType* fish_type = fish_get_type(player->fish_type);
    if (!fish_type) return;
    
    float eat_radius = fish_type->eat_radius;
    
    // Find plants within eating range
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    
    int eaten_count = 0;
    
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        
        float dx = nodes[i].x - player->x;
        float dy = nodes[i].y - player->y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance <= eat_radius) {
            // "Eat" the plant node by deactivating it
            nodes[i].active = 0;
            
            // Reduce hunger
            player->hunger -= 0.3f;
            if (player->hunger < 0.0f) player->hunger = 0.0f;
            
            eaten_count++;
            
            PlantType* plant_type = plants_get_type(nodes[i].plant_type);
            if (plant_type) {
                printf("Fish ate %s (hunger: %.2f)\n", plant_type->name, player->hunger);
            }
        }
    }
    
    if (eaten_count > 0) {
        printf("Fish ate %d plants\n", eaten_count);
    } else {
        printf("No plants in eating range\n");
    }
}

void fish_update(void) {
    for (int i = 0; i < g_fish_count; i++) {
        if (!g_fish[i].active) continue;
        
        Fish* fish = &g_fish[i];
        FishType* fish_type = fish_get_type(fish->fish_type);
        if (!fish_type) continue;
        
        // Age fish
        fish->age++;
        
        // Increase hunger
        fish->hunger += fish_type->hunger_rate;
        if (fish->hunger > 1.0f) fish->hunger = 1.0f;
        
        // Player control - accumulate forces instead of direct velocity changes
        if (fish->is_player && i == g_player_fish) {
            if (g_player_keys[0]) fish->force_y -= fish_type->acceleration; // Up
            if (g_player_keys[1]) fish->force_x -= fish_type->acceleration; // Left
            if (g_player_keys[2]) fish->force_y += fish_type->acceleration; // Down
            if (g_player_keys[3]) fish->force_x += fish_type->acceleration; // Right
        }
        
        // Note: Position and velocity updates are now handled in physics_update()
        // This function only handles fish-specific logic like hunger and player input
    }
}

Fish* fish_get_all(void) {
    return g_fish;
}

int fish_get_count(void) {
    return g_fish_count;
}

FishType* fish_get_type(int index) {
    if (index < 0 || index >= g_fish_type_count) {
        return NULL;
    }
    return &g_fish_types[index];
}

int fish_get_type_count(void) {
    return g_fish_type_count;
}