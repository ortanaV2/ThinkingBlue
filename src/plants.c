#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "plants.h"
#include "simulation.h"
#include "grid.h"
#include "nutrition.h"
#include "gas.h"

static PlantType g_plant_types[MAX_PLANT_TYPES];
static int g_plant_type_count = 0;

static void parse_color(const char* color_str, int* r, int* g, int* b) {
    const char* hex = color_str;
    if (hex[0] == '#') hex++; 
    
    unsigned int color = (unsigned int)strtoul(hex, NULL, 16);
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

int plants_load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open config file '%s'\n", filename);
        return 0;
    }
    
    char line[256];
    PlantType* current_plant = NULL;
    g_plant_type_count = 0;
    
    while (fgets(line, sizeof(line), file) && g_plant_type_count < MAX_PLANT_TYPES) {
        line[strcspn(line, "\n")] = 0;
        
        if (line[0] == '\0' || line[0] == '#') continue;
        
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            current_plant = &g_plant_types[g_plant_type_count];
            memset(current_plant, 0, sizeof(PlantType));
            
            strncpy(current_plant->name, line + 1, strlen(line) - 2);
            current_plant->name[strlen(line) - 2] = '\0';
            current_plant->active = 1;
            
            // Default values
            current_plant->growth_probability = 0.02f;
            current_plant->growth_attempts = 5;
            current_plant->max_branches = 3;
            current_plant->branch_distance = OPTIMAL_DISTANCE;
            current_plant->mobility_factor = 1.0f;
            current_plant->age_mature = 1800;
            current_plant->nutrition_depletion_strength = 0.08f;
            current_plant->nutrition_depletion_radius = 120.0f;
            current_plant->oxygen_production_factor = 0.2f;
            current_plant->oxygen_production_radius = 80.0f;
            current_plant->nutrition_value = 0.25f;  // Default nutrition value
            current_plant->node_r = 150;
            current_plant->node_g = 255;
            current_plant->node_b = 150;
            current_plant->chain_r = 100;
            current_plant->chain_g = 200;
            current_plant->chain_b = 100;
            
            g_plant_type_count++;
            continue;
        }
        
        if (!current_plant) continue;
        
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        if (strcmp(key, "growth_probability") == 0) {
            current_plant->growth_probability = (float)atof(value);
        } else if (strcmp(key, "growth_attempts") == 0) {
            current_plant->growth_attempts = atoi(value);
        } else if (strcmp(key, "max_branches") == 0) {
            current_plant->max_branches = atoi(value);
        } else if (strcmp(key, "branch_distance") == 0) {
            current_plant->branch_distance = (float)atof(value);
        } else if (strcmp(key, "mobility_factor") == 0) {
            current_plant->mobility_factor = (float)atof(value);
        } else if (strcmp(key, "age_mature") == 0) {
            current_plant->age_mature = atoi(value);
        } else if (strcmp(key, "nutrition_depletion_strength") == 0) {
            current_plant->nutrition_depletion_strength = (float)atof(value);
        } else if (strcmp(key, "nutrition_depletion_radius") == 0) {
            current_plant->nutrition_depletion_radius = (float)atof(value);
        } else if (strcmp(key, "oxygen_production_factor") == 0) {
            current_plant->oxygen_production_factor = (float)atof(value);
        } else if (strcmp(key, "oxygen_production_radius") == 0) {
            current_plant->oxygen_production_radius = (float)atof(value);
        } else if (strcmp(key, "nutrition_value") == 0) {  // FIX: Fehlender Parser!
            current_plant->nutrition_value = (float)atof(value);
        } else if (strcmp(key, "node_color") == 0) {
            parse_color(value, &current_plant->node_r, &current_plant->node_g, &current_plant->node_b);
        } else if (strcmp(key, "chain_color") == 0) {
            parse_color(value, &current_plant->chain_r, &current_plant->chain_g, &current_plant->chain_b);
        }
    }
    
    fclose(file);
    
    printf("Loaded %d plant types from config\n", g_plant_type_count);
    for (int i = 0; i < g_plant_type_count; i++) {
        PlantType* pt = &g_plant_types[i];
        printf("  %s: nutrition=%.3f, strength=%.3f, radius=%.1f\n",
               pt->name, pt->nutrition_value, pt->nutrition_depletion_strength, pt->nutrition_depletion_radius);
    }
    
    return g_plant_type_count > 0;
}

static int is_position_free(float x, float y, float min_distance) {
    float min_dist_sq = min_distance * min_distance;
    
    GridCell* cells[9];
    int cell_count = grid_get_cells_at_position(x, y, cells, 9);
    
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    
    for (int c = 0; c < cell_count; c++) {
        GridCell* cell = cells[c];
        if (!cell) continue;
        
        for (int k = 0; k < cell->count; k++) {
            int i = cell->node_indices[k];
            if (i < 0 || i >= node_count) continue;
            if (!nodes[i].active) continue;
            
            float dx = nodes[i].x - x;
            float dy = nodes[i].y - y;
            float distance_sq = dx * dx + dy * dy;
            
            if (distance_sq < min_dist_sq) {
                return 0;
            }
        }
    }
    return 1;
}

static float calculate_nutrition_growth_modifier(float nutrition_value) {
    if (nutrition_value < 0.2f) {
        return 0.05f;
    } else if (nutrition_value < 0.3f) {
        return 0.05f + (nutrition_value - 0.2f) / 0.1f * 0.05f;
    } else if (nutrition_value < 0.4f) {
        return 0.1f + (nutrition_value - 0.3f) / 0.1f * 0.15f;
    } else if (nutrition_value < 0.5f) {
        return 0.25f + (nutrition_value - 0.4f) / 0.1f * 0.25f;
    } else if (nutrition_value < 0.6f) {
        return 0.5f + (nutrition_value - 0.5f) / 0.1f * 0.5f;
    } else if (nutrition_value < 0.7f) {
        return 1.0f + (nutrition_value - 0.6f) / 0.1f * 0.8f;
    } else if (nutrition_value < 0.8f) {
        return 1.8f + (nutrition_value - 0.7f) / 0.1f * 0.7f;
    } else {
        return 2.5f + (nutrition_value - 0.8f) / 0.2f * 1.0f;
    }
}

void plants_grow(void) {
    Node* nodes = simulation_get_nodes();
    int current_node_count = simulation_get_node_count();
    
    nutrition_regenerate();
    gas_update_heatmap();
    
    int growth_limit = (current_node_count / 100) + 3;
    if (growth_limit > 50) growth_limit = 50;
    
    int grown = 0;
    
    for (int i = 0; i < current_node_count && grown < growth_limit; i++) {
        if (!nodes[i].active || !nodes[i].can_grow) continue;
        
        int plant_type = nodes[i].plant_type;
        if (plant_type < 0 || plant_type >= g_plant_type_count) continue;
        
        PlantType* pt = &g_plant_types[plant_type];
        
        if (nodes[i].branch_count >= pt->max_branches) continue;
        if (nodes[i].age > pt->age_mature) continue;
        
        float nutrition_value = nutrition_get_value_at(nodes[i].x, nodes[i].y);
        float nutrition_modifier = calculate_nutrition_growth_modifier(nutrition_value);
        
        float modified_growth_prob = pt->growth_probability * nutrition_modifier;
        
        if ((float)rand() / RAND_MAX < modified_growth_prob) {
            float attempt_modifier = nutrition_modifier;
            if (nutrition_value < 0.3f) {
                attempt_modifier *= 0.3f;
            } else if (nutrition_value > 0.7f) {
                attempt_modifier *= 1.8f;
            }
            
            int modified_attempts = (int)(pt->growth_attempts * attempt_modifier);
            if (modified_attempts < 1) modified_attempts = 1;
            if (modified_attempts > pt->growth_attempts * 3) modified_attempts = pt->growth_attempts * 3;
            
            for (int attempt = 0; attempt < modified_attempts; attempt++) {
                float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
                float new_x = nodes[i].x + cos(angle) * pt->branch_distance;
                float new_y = nodes[i].y + sin(angle) * pt->branch_distance;
                
                if (new_x < WORLD_LEFT || new_x > WORLD_RIGHT ||
                    new_y < WORLD_TOP || new_y > WORLD_BOTTOM) {
                    continue;
                }
                
                if (is_position_free(new_x, new_y, pt->branch_distance * 0.8f)) {
                    int new_node = simulation_add_node(new_x, new_y, plant_type);
                    if (new_node >= 0) {
                        simulation_add_chain(i, new_node);
                        nodes[i].branch_count++;
                        grown++;
                        
                        // Calculate and apply nutrition depletion for new plant
                        float depletion_strength = pt->nutrition_depletion_strength;
                        float depletion_radius = pt->nutrition_depletion_radius;
                        
                        float size_factor = (pt->max_branches / 3.0f) * (pt->branch_distance / OPTIMAL_DISTANCE);
                        float actual_depletion = depletion_strength * size_factor;
                        
                        // Apply nutrition depletion to environment
                        nutrition_deplete_at_position(new_x, new_y, actual_depletion, depletion_radius);
                        
                        break;
                    }
                }
            }
        }
    }
}

int plants_get_type_count(void) {
    return g_plant_type_count;
}

PlantType* plants_get_type(int index) {
    if (index < 0 || index >= g_plant_type_count) {
        return NULL;
    }
    return &g_plant_types[index];
}

// REMOVED: Old nutrition cost tracking functions (no longer needed)
float plants_get_nutrition_cost_for_node(int node_id) {
    // This function is kept for compatibility but returns 0
    // The new system calculates nutrition directly in fish.c
    return 0.0f;
}

float plants_get_total_nutrition_cost(void) {
    // This function is kept for compatibility but returns 0
    // The new system doesn't track total plant costs
    return 0.0f;
}