// fish_core.c - Enhanced core fish system with aging, death, and corpse creation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"

// Global fish state
static Fish* g_fish = NULL;
static FishType g_fish_types[MAX_FISH_TYPES];
static int g_fish_count = 0;
static int g_fish_type_count = 0;
static int g_ray_rendering_enabled = 0;

// Nutrition tracking
static float g_total_nutrition_consumed = 0.0f;
static float g_total_nutrition_defecated = 0.0f;

// Death and corpse statistics
static int g_total_deaths_from_age = 0;
static int g_total_corpses_created = 0;
static int g_total_corpses_eaten = 0;

// Color parsing utility
static void parse_color(const char* color_str, int* r, int* g, int* b) {
    const char* hex = color_str;
    if (hex[0] == '#') hex++; 
    
    unsigned int color = (unsigned int)strtoul(hex, NULL, 16);
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

// Create corpse from dead fish
static int create_corpse(int fish_id, float x, float y, int fish_type, float heading) {
    int corpse_node = simulation_add_node(x, y, -2);  // -2 indicates corpse
    if (corpse_node < 0) {
        return -1;
    }
    
    Node* nodes = simulation_get_nodes();
    Node* corpse = &nodes[corpse_node];
    
    corpse->is_corpse = 1;
    corpse->corpse_decay_timer = CORPSE_DECAY_TIME;
    corpse->original_fish_type = fish_type;
    corpse->corpse_heading = heading;  // Preserve fish heading for tail rendering
    corpse->vx = 0.0f;  // Corpses don't move
    corpse->vy = 0.0f;
    
    g_total_corpses_created++;
    
    FishType* ft = fish_get_type(fish_type);
    printf("Corpse created from fish %d (%s) at (%.0f, %.0f) - will decay in %d frames\n",
           fish_id, ft ? ft->name : "Unknown", x, y, CORPSE_DECAY_TIME);
    
    return corpse_node;
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
    g_total_nutrition_consumed = 0.0f;
    g_total_nutrition_defecated = 0.0f;
    g_total_deaths_from_age = 0;
    g_total_corpses_created = 0;
    g_total_corpses_eaten = 0;
    
    printf("Fish system initialized with aging mechanics and corpse system\n");
    return 1;
}

void fish_cleanup(void) {
    if (g_fish) {
        free(g_fish);
        g_fish = NULL;
    }
    g_fish_count = 0;
    g_fish_type_count = 0;
    
    printf("Fish cleanup - Deaths: %d, Corpses created: %d, Corpses eaten: %d, Consumed: %.2f, Defecated: %.2f\n",
           g_total_deaths_from_age, g_total_corpses_created, g_total_corpses_eaten,
           g_total_nutrition_consumed, g_total_nutrition_defecated);
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
    
    printf("Loading fish config with aging system and corpse mechanics from '%s'\n", filename);
    
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
                
                // Default values
                current_fish->max_speed = 15.0f;
                current_fish->max_force = 3.0f;
                current_fish->mass = 1.0f;
                current_fish->size_radius = 8.0f;
                current_fish->eating_range = 70.0f;
                current_fish->fov_angle = 200.0f;
                current_fish->max_turn_angle = 45.0f;
                current_fish->oxygen_reward_factor = 0.01f;
                current_fish->proximity_reward_factor = 0.005f;
                current_fish->eat_punishment = -0.02f;
                current_fish->flow_sensitivity = 0.2f;
                current_fish->danger_level = 0.1f;
                current_fish->is_predator = 0;
                current_fish->eating_cooldown_frames = 0;
                current_fish->fish_detection_range = 300.0f;
                current_fish->max_age = 18000;  // Default: 10 minutes at 30 FPS
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
        } else if (strcmp(key, "max_force") == 0) {
            current_fish->max_force = (float)atof(value);
        } else if (strcmp(key, "mass") == 0) {
            current_fish->mass = (float)atof(value);
        } else if (strcmp(key, "size_radius") == 0) {
            current_fish->size_radius = (float)atof(value);
        } else if (strcmp(key, "eating_range") == 0) {
            current_fish->eating_range = (float)atof(value);
        } else if (strcmp(key, "fov_angle") == 0) {
            current_fish->fov_angle = (float)atof(value);
        } else if (strcmp(key, "max_turn_angle") == 0) {
            current_fish->max_turn_angle = (float)atof(value);
        } else if (strcmp(key, "oxygen_reward_factor") == 0) {
            current_fish->oxygen_reward_factor = (float)atof(value);
        } else if (strcmp(key, "proximity_reward_factor") == 0) {
            current_fish->proximity_reward_factor = (float)atof(value);
        } else if (strcmp(key, "eat_punishment") == 0) {
            current_fish->eat_punishment = (float)atof(value);
        } else if (strcmp(key, "flow_sensitivity") == 0) {
            current_fish->flow_sensitivity = (float)atof(value);
        } else if (strcmp(key, "danger_level") == 0) {
            current_fish->danger_level = (float)atof(value);
        } else if (strcmp(key, "is_predator") == 0) {
            current_fish->is_predator = atoi(value);
        } else if (strcmp(key, "eating_cooldown_frames") == 0) {
            current_fish->eating_cooldown_frames = atoi(value);
        } else if (strcmp(key, "fish_detection_range") == 0) {
            current_fish->fish_detection_range = (float)atof(value);
        } else if (strcmp(key, "max_age") == 0) {
            current_fish->max_age = atoi(value);
        } else if (strcmp(key, "node_color") == 0) {
            parse_color(value, &current_fish->node_r, &current_fish->node_g, &current_fish->node_b);
        }
    }
    
    fclose(file);
    
    printf("Loaded %d fish types with aging system and corpse mechanics\n", g_fish_type_count);
    
    for (int i = 0; i < g_fish_type_count; i++) {
        FishType* ft = &g_fish_types[i];
        float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
        printf("  %s: Max age %d frames (%.1f min), Danger %.1f, %s\n",
               ft->name, ft->max_age, lifespan_minutes, ft->danger_level, 
               ft->is_predator ? "PREDATOR" : "HERBIVORE");
    }
    
    return g_fish_type_count > 0;
}

int fish_add(float x, float y, int fish_type) {
    if (g_fish_count >= MAX_FISH) {
        printf("Error: Maximum fish count reached (%d)\n", MAX_FISH);
        return -1;
    }
    
    if (fish_type < 0 || fish_type >= g_fish_type_count) {
        printf("Error: Invalid fish type %d\n", fish_type);
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
    
    // Initialize RL system
    fish->heading = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
    for (int i = 0; i < RL_INPUT_SIZE; i++) {
        fish->rl_inputs[i] = 0.0f;
    }
    for (int i = 0; i < RL_OUTPUT_SIZE; i++) {
        fish->rl_outputs[i] = 0.0f;
    }
    
    // Initialize state
    fish->energy = 1.0f;
    fish->stomach_contents = 0.0f;
    fish->consumed_nutrition = 0.0f;
    fish->last_eating_frame = 0;
    fish->age = 0;
    fish->active = 1;
    fish->eating_mode = 0;
    fish->total_reward = 0.0f;
    fish->last_reward = 0.0f;
    fish->defecation_count = 0;
    fish->eating_cooldown = 0;
    fish->target_fish_id = -1;
    
    // Initialize aging system
    fish->birth_frame = simulation_get_frame_counter();
    
    int fish_id = g_fish_count;
    g_fish_count++;
    
    FishType* ft = &g_fish_types[fish_type];
    float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
    printf("Created %s fish %d (max age: %.1f min)\n", 
           ft->name, fish_id, lifespan_minutes);
    
    return fish_id;
}

void fish_remove(int fish_id) {
    if (fish_id < 0 || fish_id >= g_fish_count) return;
    
    FishType* ft = fish_get_type(g_fish[fish_id].fish_type);
    printf("Removed fish %d (%s)\n", fish_id, ft ? ft->name : "?");
    
    g_fish[fish_id].active = 0;
}

// Check if fish should die from age
int fish_should_die_from_age(int fish_id) {
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return 0;
    
    int current_frame = simulation_get_frame_counter();
    
    // Check every 30 frames based on birth frame offset
    if ((current_frame - fish->birth_frame) % DEATH_CHECK_INTERVAL != 0) {
        return 0;
    }
    
    int age = current_frame - fish->birth_frame;
    
    // Calculate death probability based on age
    float age_ratio = (float)age / (float)fish_type->max_age;
    float death_probability;
    
    if (age_ratio <= 0.5f) {
        // Young fish: very low probability (0% to 2%)
        death_probability = age_ratio * age_ratio * 0.08f;
    } else if (age_ratio <= 1.0f) {
        // Middle age: probability increases to 50% at max_age
        float x = (age_ratio - 0.5f) * 2.0f;
        death_probability = 0.02f + x * x * 0.48f;
    } else {
        // Old fish: rapidly increasing probability
        float excess = age_ratio - 1.0f;
        death_probability = 0.5f + excess * 0.8f;
        if (death_probability > 0.95f) death_probability = 0.95f;
    }
    
    // Random roll
    float roll = (float)rand() / RAND_MAX;
    
    if (roll < death_probability) {
        // Create corpse before marking fish as dead
        Node* nodes = simulation_get_nodes();
        Node* fish_node = &nodes[fish->node_id];
        
        create_corpse(fish_id, fish_node->x, fish_node->y, fish->fish_type, fish->heading);
        
        g_total_deaths_from_age++;
        
        float age_minutes = age / (TARGET_FPS * 60.0f);
        float max_age_minutes = fish_type->max_age / (TARGET_FPS * 60.0f);
        
        printf("Fish %d (%s) died from age: %.1f/%.1f min (%.0f%% probability) - corpse created\n",
               fish_id, fish_type->name, age_minutes, max_age_minutes, 
               death_probability * 100.0f);
        
        return 1;
    }
    
    return 0;
}

// Get death statistics
int fish_get_total_deaths_from_age(void) {
    return g_total_deaths_from_age;
}

// Get corpse statistics
int fish_get_total_corpses_created(void) {
    return g_total_corpses_created;
}

int fish_get_total_corpses_eaten(void) {
    return g_total_corpses_eaten;
}

void fish_increment_corpses_eaten(void) {
    g_total_corpses_eaten++;
}

// Accessor functions
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

// Nutrition tracking
float fish_get_total_nutrition_consumed(void) {
    return g_total_nutrition_consumed;
}

float fish_get_total_nutrition_defecated(void) {
    return g_total_nutrition_defecated;
}

float fish_get_nutrition_balance(void) {
    return g_total_nutrition_consumed - g_total_nutrition_defecated;
}

// Internal access for other fish modules
Fish* fish_internal_get_array(void) {
    return g_fish;
}

FishType* fish_internal_get_types(void) {
    return g_fish_types;
}

void fish_internal_add_consumed_nutrition(float amount) {
    g_total_nutrition_consumed += amount;
}

void fish_internal_add_defecated_nutrition(float amount) {
    g_total_nutrition_defecated += amount;
}