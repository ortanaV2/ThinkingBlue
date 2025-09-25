// fish_core.c - Enhanced fish system with visual configuration
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "fish.h"
#include "simulation.h"

// Global fish state with robust tracking
static Fish* g_fish = NULL;
static FishType g_fish_types[MAX_FISH_TYPES];
static int g_fish_type_count = 0;
static int g_ray_rendering_enabled = 0;

// Robust ID tracking
static int g_highest_used_slot = -1;
static int g_active_fish_count = 0;

// Nutrition tracking
static float g_total_nutrition_consumed = 0.0f;
static float g_total_nutrition_defecated = 0.0f;

// Death and corpse statistics
static int g_total_deaths_from_age = 0;
static int g_total_corpses_created = 0;
static int g_total_corpses_eaten = 0;

// Parse color from hex string (e.g., "FF6B35")
static void parse_color(const char* color_str, int* r, int* g, int* b) {
    const char* hex = color_str;
    if (hex[0] == '#') hex++; 
    
    unsigned int color = (unsigned int)strtoul(hex, NULL, 16);
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

// Update cached fish count and highest slot tracker for efficient iteration
static void update_fish_tracking(void) {
    g_active_fish_count = 0;
    g_highest_used_slot = -1;
    
    for (int i = 0; i < MAX_FISH; i++) {
        if (g_fish[i].active) {
            g_active_fish_count++;
            g_highest_used_slot = i;
        }
    }
}

// Validate fish ID and return fish pointer if valid
static Fish* get_validated_fish(int fish_id) {
    if (fish_id < 0 || fish_id >= MAX_FISH) {
        return NULL;
    }
    if (!g_fish[fish_id].active) {
        return NULL;
    }
    return &g_fish[fish_id];
}

// Create corpse from dead fish with preserved heading
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
    corpse->corpse_heading = heading;
    corpse->vx = 0.0f;
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
    
    // Initialize all fish as inactive with clear IDs
    for (int i = 0; i < MAX_FISH; i++) {
        g_fish[i].active = 0;
        g_fish[i].node_id = -1;
        g_fish[i].fish_type = -1;
    }
    
    // Reset tracking variables
    g_fish_type_count = 0;
    g_ray_rendering_enabled = 0;
    g_active_fish_count = 0;
    g_highest_used_slot = -1;
    
    // Reset statistics
    g_total_nutrition_consumed = 0.0f;
    g_total_nutrition_defecated = 0.0f;
    g_total_deaths_from_age = 0;
    g_total_corpses_created = 0;
    g_total_corpses_eaten = 0;
    
    printf("Fish system initialized with enhanced visual configuration (max %d fish)\n", MAX_FISH);
    return 1;
}

void fish_cleanup(void) {
    if (g_fish) {
        free(g_fish);
        g_fish = NULL;
    }
    
    printf("Fish cleanup - Active: %d, Deaths: %d, Corpses created: %d, Corpses eaten: %d\n",
           g_active_fish_count, g_total_deaths_from_age, g_total_corpses_created, g_total_corpses_eaten);
    printf("Nutrition - Consumed: %.2f, Defecated: %.2f, Balance: %.2f\n",
           g_total_nutrition_consumed, g_total_nutrition_defecated, 
           g_total_nutrition_consumed - g_total_nutrition_defecated);
    
    g_active_fish_count = 0;
    g_highest_used_slot = -1;
    g_fish_type_count = 0;
}

void fish_toggle_ray_rendering(void) {
    g_ray_rendering_enabled = !g_ray_rendering_enabled;
    printf("Fish ray rendering: %s\n", g_ray_rendering_enabled ? "ON" : "OFF");
}

int fish_is_ray_rendering_enabled(void) {
    return g_ray_rendering_enabled;
}

// Load fish configuration with enhanced visual parameters
int fish_load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open fish config file '%s'\n", filename);
        return 0;
    }
    
    printf("Loading fish config with visual enhancement from '%s'\n", filename);
    
    char line[256];
    FishType* current_fish = NULL;
    g_fish_type_count = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (g_fish_type_count >= MAX_FISH_TYPES) {
            printf("Warning: Maximum fish types reached (%d)\n", MAX_FISH_TYPES);
            break;
        }
        
        line[strcspn(line, "\r\n")] = 0;
        
        // Skip empty lines and comments
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        // Parse fish type section headers
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            current_fish = &g_fish_types[g_fish_type_count];
            memset(current_fish, 0, sizeof(FishType));
            
            int name_len = strlen(line) - 2;
            if (name_len > 0 && name_len < MAX_NAME_LENGTH) {
                strncpy(current_fish->name, line + 1, name_len);
                current_fish->name[name_len] = '\0';
                current_fish->active = 1;
                
                // Set default values for all parameters
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
                current_fish->max_age = 18000;
                
                // Default visual configuration
                current_fish->node_size_factor = 1.0f;
                current_fish->tail_length_factor = 1.0f;
                current_fish->tail_width_factor = 1.0f;
                
                // Default color (orange)
                current_fish->node_r = 255;
                current_fish->node_g = 165;
                current_fish->node_b = 0;
                
                g_fish_type_count++;
            }
            continue;
        }
        
        if (!current_fish) continue;
        
        // Parse key-value pairs
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
        
        // Parse configuration parameters
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
        } 
        // Enhanced visual configuration parameters
        else if (strcmp(key, "node_size_factor") == 0) {
            current_fish->node_size_factor = (float)atof(value);
            // Clamp to reasonable range
            if (current_fish->node_size_factor < 0.1f) current_fish->node_size_factor = 0.1f;
            if (current_fish->node_size_factor > 3.0f) current_fish->node_size_factor = 3.0f;
        } else if (strcmp(key, "tail_length_factor") == 0) {
            current_fish->tail_length_factor = (float)atof(value);
            // Clamp to reasonable range
            if (current_fish->tail_length_factor < 0.1f) current_fish->tail_length_factor = 0.1f;
            if (current_fish->tail_length_factor > 3.0f) current_fish->tail_length_factor = 3.0f;
        } else if (strcmp(key, "tail_width_factor") == 0) {
            current_fish->tail_width_factor = (float)atof(value);
            // Clamp to reasonable range
            if (current_fish->tail_width_factor < 0.1f) current_fish->tail_width_factor = 0.1f;
            if (current_fish->tail_width_factor > 3.0f) current_fish->tail_width_factor = 3.0f;
        } else if (strcmp(key, "node_color") == 0) {
            parse_color(value, &current_fish->node_r, &current_fish->node_g, &current_fish->node_b);
        }
    }
    
    fclose(file);
    
    printf("Loaded %d fish types with enhanced visual configuration\n", g_fish_type_count);
    
    // Display loaded fish types with their visual settings
    for (int i = 0; i < g_fish_type_count; i++) {
        FishType* ft = &g_fish_types[i];
        float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
        printf("  %s: Max age %d frames (%.1f min), Danger %.1f, %s\n",
               ft->name, ft->max_age, lifespan_minutes, ft->danger_level, 
               ft->is_predator ? "PREDATOR" : "HERBIVORE");
        printf("    Visual: Size %.1f, Tail L%.1f W%.1f, Color(%d,%d,%d)\n",
               ft->node_size_factor, ft->tail_length_factor, ft->tail_width_factor,
               ft->node_r, ft->node_g, ft->node_b);
    }
    
    return g_fish_type_count > 0;
}

// Find first available slot and create fish with enhanced visual properties
int fish_add(float x, float y, int fish_type) {
    // Validate fish type first
    if (fish_type < 0 || fish_type >= g_fish_type_count) {
        printf("Error: Invalid fish type %d (available: 0-%d)\n", fish_type, g_fish_type_count - 1);
        return -1;
    }
    
    // Find first available slot
    int fish_id = -1;
    for (int i = 0; i < MAX_FISH; i++) {
        if (!g_fish[i].active) {
            fish_id = i;
            break;
        }
    }
    
    if (fish_id == -1) {
        printf("Error: Maximum fish count reached (%d active of %d max)\n", g_active_fish_count, MAX_FISH);
        return -1;
    }
    
    // Create simulation node
    int node_id = simulation_add_node(x, y, -1);
    if (node_id < 0) {
        printf("Error: Failed to create simulation node for fish %d\n", fish_id);
        return -1;
    }
    
    // Initialize fish in found slot
    Fish* fish = &g_fish[fish_id];
    memset(fish, 0, sizeof(Fish));  // Clear all data first
    
    // Set core data
    fish->active = 1;               // Activate immediately
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
    
    // Initialize state variables
    fish->energy = 1.0f;
    fish->stomach_contents = 0.0f;
    fish->consumed_nutrition = 0.0f;
    fish->last_eating_frame = 0;
    fish->age = 0;
    fish->eating_mode = 0;
    fish->total_reward = 0.0f;
    fish->last_reward = 0.0f;
    fish->defecation_count = 0;
    fish->eating_cooldown = 0;
    fish->target_fish_id = -1;
    fish->birth_frame = simulation_get_frame_counter();
    
    // Update tracking
    update_fish_tracking();
    
    FishType* ft = &g_fish_types[fish_type];
    float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
    printf("Created %s fish ID=%d at (%.0f,%.0f) - Active:%d/%d, Max age: %.1f min\n", 
           ft->name, fish_id, x, y, g_active_fish_count, MAX_FISH, lifespan_minutes);
    
    return fish_id;
}

// Remove fish with proper cleanup
void fish_remove(int fish_id) {
    Fish* fish = get_validated_fish(fish_id);
    if (!fish) {
        printf("Warning: Attempted to remove invalid fish ID %d\n", fish_id);
        return;
    }
    
    FishType* ft = fish_get_type(fish->fish_type);
    printf("Removing fish %d (%s)\n", fish_id, ft ? ft->name : "Unknown");
    
    // Deactivate simulation node if it exists
    Node* nodes = simulation_get_nodes();
    if (fish->node_id >= 0 && fish->node_id < simulation_get_node_count()) {
        nodes[fish->node_id].active = 0;
    }
    
    // Clear fish data
    fish->active = 0;
    fish->node_id = -1;
    fish->fish_type = -1;
    
    // Update tracking
    update_fish_tracking();
    
    printf("Fish %d removed - Active fish remaining: %d\n", fish_id, g_active_fish_count);
}

// Check if fish should die from age with probability curve
int fish_should_die_from_age(int fish_id) {
    Fish* fish = get_validated_fish(fish_id);
    if (!fish) return 0;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return 0;
    
    int current_frame = simulation_get_frame_counter();
    
    // Check every 30 frames based on birth frame offset to spread death checks
    if ((current_frame - fish->birth_frame) % DEATH_CHECK_INTERVAL != 0) {
        return 0;
    }
    
    int age = current_frame - fish->birth_frame;
    
    // Calculate death probability based on age curve
    float age_ratio = (float)age / (float)fish_type->max_age;
    float death_probability;
    
    if (age_ratio <= 0.5f) {
        // Young fish: very low death probability
        death_probability = age_ratio * age_ratio * 0.08f;
    } else if (age_ratio <= 1.0f) {
        // Middle-aged to old: increasing probability
        float x = (age_ratio - 0.5f) * 2.0f;
        death_probability = 0.02f + x * x * 0.48f;
    } else {
        // Very old fish: high death probability
        float excess = age_ratio - 1.0f;
        death_probability = 0.5f + excess * 0.8f;
        if (death_probability > 0.95f) death_probability = 0.95f;
    }
    
    // Random roll for death
    float roll = (float)rand() / RAND_MAX;
    
    if (roll < death_probability) {
        // Create corpse before marking fish as dead
        Node* nodes = simulation_get_nodes();
        if (fish->node_id >= 0 && fish->node_id < simulation_get_node_count()) {
            Node* fish_node = &nodes[fish->node_id];
            create_corpse(fish_id, fish_node->x, fish_node->y, fish->fish_type, fish->heading);
        }
        
        g_total_deaths_from_age++;
        
        float age_minutes = age / (TARGET_FPS * 60.0f);
        float max_age_minutes = fish_type->max_age / (TARGET_FPS * 60.0f);
        
        printf("Fish %d (%s) died from age: %.1f/%.1f min (%.0f%% probability)\n",
               fish_id, fish_type->name, age_minutes, max_age_minutes, 
               death_probability * 100.0f);
        
        return 1;
    }
    
    return 0;
}

// Accessor functions for robust fish system
int fish_get_count(void) {
    return g_active_fish_count;
}

int fish_get_highest_slot(void) {
    return g_highest_used_slot;
}

Fish* fish_get_by_id(int fish_id) {
    return get_validated_fish(fish_id);
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

// Statistics accessors
int fish_get_total_deaths_from_age(void) {
    return g_total_deaths_from_age;
}

int fish_get_total_corpses_created(void) {
    return g_total_corpses_created;
}

int fish_get_total_corpses_eaten(void) {
    return g_total_corpses_eaten;
}

void fish_increment_corpses_eaten(void) {
    g_total_corpses_eaten++;
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

// Internal access functions for other fish modules
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

// Debug function to print fish system status
void fish_debug_print_status(void) {
    printf("\n=== FISH SYSTEM DEBUG STATUS ===\n");
    printf("Active fish count: %d\n", g_active_fish_count);
    printf("Highest used slot: %d\n", g_highest_used_slot);
    printf("Fish types loaded: %d\n", g_fish_type_count);
    
    int active_count = 0;
    int highest_active = -1;
    
    // Verify tracking accuracy
    for (int i = 0; i <= g_highest_used_slot && i < MAX_FISH; i++) {
        if (g_fish[i].active) {
            active_count++;
            highest_active = i;
            printf("  Slot %d: %s (node=%d, type=%d)\n", 
                   i, 
                   (g_fish[i].fish_type >= 0 && g_fish[i].fish_type < g_fish_type_count) ? 
                   g_fish_types[g_fish[i].fish_type].name : "INVALID_TYPE",
                   g_fish[i].node_id, 
                   g_fish[i].fish_type);
        }
    }
    
    printf("Verification: Found %d active fish, highest at slot %d\n", active_count, highest_active);
    
    // Fix tracking if mismatch detected
    if (active_count != g_active_fish_count) {
        printf("ERROR: Count mismatch! Cached=%d, Actual=%d\n", g_active_fish_count, active_count);
        update_fish_tracking();  // Fix it
        printf("Fixed: Cached count now = %d\n", g_active_fish_count);
    }
    
    printf("==================================\n\n");
}