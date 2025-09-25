// plants.c - Complete fixed nutrition system for all plant types
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
#include "temperature.h"

static PlantType g_plant_types[MAX_PLANT_TYPES];
static int g_plant_type_count = 0;

// Track environmental nutrition balance (what's in the ground)
static float g_environmental_nutrition_balance = 0.0f;
static float g_initial_environmental_nutrition = 0.0f;
static int g_initial_nutrition_calculated = 0;

// Parse hex color string to RGB values
static void parse_color(const char* color_str, int* r, int* g, int* b) {
    const char* hex = color_str;
    if (hex[0] == '#') hex++; 
    
    unsigned int color = (unsigned int)strtoul(hex, NULL, 16);
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

// Calculate initial nutrition in the layer (called once)
static void calculate_initial_environmental_nutrition(void) {
    if (g_initial_nutrition_calculated) return;
    
    // Sum all nutrition values in the layer
    int grid_width = (int)ceil(WORLD_WIDTH / LAYER_GRID_SIZE);
    int grid_height = (int)ceil(WORLD_HEIGHT / LAYER_GRID_SIZE);
    
    g_initial_environmental_nutrition = 0.0f;
    for (int y = 0; y < grid_height; y++) {
        for (int x = 0; x < grid_width; x++) {
            float world_x = WORLD_LEFT + x * LAYER_GRID_SIZE + LAYER_GRID_SIZE * 0.5f;
            float world_y = WORLD_TOP + y * LAYER_GRID_SIZE + LAYER_GRID_SIZE * 0.5f;
            
            float nutrition_value = nutrition_get_value_at(world_x, world_y);
            g_initial_environmental_nutrition += nutrition_value;
        }
    }
    
    g_environmental_nutrition_balance = 0.0f; // Start at 0 balance
    g_initial_nutrition_calculated = 1;
    
    printf("Initial environmental nutrition calculated: %.2f\n", g_initial_environmental_nutrition);
}

// FIXED: Calculate nutrition cost for a plant based on its properties
static float calculate_plant_nutrition_cost(PlantType* pt) {
    if (!pt) return 0.08f;
    
    // Calculate based on plant size and complexity
    float size_factor = (pt->max_branches / 3.0f) * (pt->branch_distance / OPTIMAL_DISTANCE);
    float nutrition_cost = pt->nutrition_depletion_strength * size_factor;
    
    return nutrition_cost;
}

// Load plant configuration from file
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
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;
        
        // Parse plant type section headers
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            current_plant = &g_plant_types[g_plant_type_count];
            memset(current_plant, 0, sizeof(PlantType));
            
            strncpy(current_plant->name, line + 1, strlen(line) - 2);
            current_plant->name[strlen(line) - 2] = '\0';
            current_plant->active = 1;
            
            // Set default values for all parameters
            current_plant->growth_probability = 0.002f;
            current_plant->growth_attempts = 5;
            current_plant->max_branches = 3;
            current_plant->branch_distance = OPTIMAL_DISTANCE;
            current_plant->mobility_factor = 1.0f;
            current_plant->age_mature = 1800;
            current_plant->nutrition_depletion_strength = 0.08f;
            current_plant->oxygen_production_factor = 0.2f;
            current_plant->oxygen_production_radius = 80.0f;
            
            // Visual defaults
            current_plant->node_size_factor = 1.0f;
            current_plant->chain_thickness_factor = 1.0f;
            current_plant->chain_curvature_factor = 1.0f;
            
            // Default colors (green)
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
        
        // Parse key-value pairs
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Trim whitespace
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        // Parse configuration parameters
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
        } else if (strcmp(key, "oxygen_production_factor") == 0) {
            current_plant->oxygen_production_factor = (float)atof(value);
        } else if (strcmp(key, "oxygen_production_radius") == 0) {
            current_plant->oxygen_production_radius = (float)atof(value);
        } else if (strcmp(key, "node_size_factor") == 0) {
            current_plant->node_size_factor = (float)atof(value);
            if (current_plant->node_size_factor < 0.1f) current_plant->node_size_factor = 0.1f;
            if (current_plant->node_size_factor > 5.0f) current_plant->node_size_factor = 5.0f;
        } else if (strcmp(key, "chain_thickness_factor") == 0) {
            current_plant->chain_thickness_factor = (float)atof(value);
            if (current_plant->chain_thickness_factor < 0.1f) current_plant->chain_thickness_factor = 0.1f;
            if (current_plant->chain_thickness_factor > 5.0f) current_plant->chain_thickness_factor = 5.0f;
        } else if (strcmp(key, "chain_curvature_factor") == 0) {
            current_plant->chain_curvature_factor = (float)atof(value);
            if (current_plant->chain_curvature_factor < 0.0f) current_plant->chain_curvature_factor = 0.0f;
            if (current_plant->chain_curvature_factor > 3.0f) current_plant->chain_curvature_factor = 3.0f;
        } else if (strcmp(key, "node_color") == 0) {
            parse_color(value, &current_plant->node_r, &current_plant->node_g, &current_plant->node_b);
        } else if (strcmp(key, "chain_color") == 0) {
            parse_color(value, &current_plant->chain_r, &current_plant->chain_g, &current_plant->chain_b);
        }
    }
    
    fclose(file);
    
    printf("Loaded %d plant types with fixed nutrition system\n", g_plant_type_count);
    printf("Standard depletion range: %.1f, gradient: %.1f\n", 
           STANDARD_DEPLETION_RANGE, NUTRITION_RANGE_GRADIENT);
    
    // Display loaded plant types with their nutrition costs
    for (int i = 0; i < g_plant_type_count; i++) {
        PlantType* pt = &g_plant_types[i];
        float nutrition_cost = calculate_plant_nutrition_cost(pt);
        int is_coral = strstr(pt->name, "Coral") != NULL ? 1 : 0;
        printf("  %s: nutrition=%.3f, visual(%.1f,%.1f,%.1f), %s\n",
               pt->name, nutrition_cost, 
               pt->node_size_factor, pt->chain_thickness_factor, pt->chain_curvature_factor,
               is_coral ? "CORAL" : "NON-CORAL");
    }
    
    return g_plant_type_count > 0;
}

// Check if position is free for new plant growth
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

// Calculate growth modifier based on nutrition availability
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

// Main plant growth function
void plants_grow(void) {
    // Calculate initial nutrition if not done yet
    calculate_initial_environmental_nutrition();
    
    Node* nodes = simulation_get_nodes();
    int current_node_count = simulation_get_node_count();
    
    // Update environmental systems
    nutrition_regenerate();
    gas_update_heatmap();
    
    // Limit growth per frame for performance
    int growth_limit = (current_node_count / 100) + 3;
    if (growth_limit > 50) growth_limit = 50;
    
    int grown = 0;
    
    for (int i = 0; i < current_node_count && grown < growth_limit; i++) {
        if (!nodes[i].active || !nodes[i].can_grow) continue;
        
        int plant_type = nodes[i].plant_type;
        if (plant_type < 0 || plant_type >= g_plant_type_count) continue;
        
        PlantType* pt = &g_plant_types[plant_type];
        
        // Check growth constraints
        if (nodes[i].branch_count >= pt->max_branches) continue;
        if (nodes[i].age > pt->age_mature) continue;
        
        // Skip bleached corals (they can't grow)
        if (temperature_is_coral_bleached(i)) {
            continue;
        }
        
        // Calculate growth probability based on local nutrition
        float nutrition_value = nutrition_get_value_at(nodes[i].x, nodes[i].y);
        float nutrition_modifier = calculate_nutrition_growth_modifier(nutrition_value);
        float modified_growth_prob = pt->growth_probability * nutrition_modifier;
        
        // Roll for growth
        if ((float)rand() / RAND_MAX < modified_growth_prob) {
            // Calculate growth attempts based on nutrition
            float attempt_modifier = nutrition_modifier;
            if (nutrition_value < 0.3f) {
                attempt_modifier *= 0.3f;
            } else if (nutrition_value > 0.7f) {
                attempt_modifier *= 1.8f;
            }
            
            int modified_attempts = (int)(pt->growth_attempts * attempt_modifier);
            if (modified_attempts < 1) modified_attempts = 1;
            if (modified_attempts > pt->growth_attempts * 3) modified_attempts = pt->growth_attempts * 3;
            
            // Try to grow in random directions
            for (int attempt = 0; attempt < modified_attempts; attempt++) {
                float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
                float new_x = nodes[i].x + cos(angle) * pt->branch_distance;
                float new_y = nodes[i].y + sin(angle) * pt->branch_distance;
                
                // Check world bounds
                if (new_x < WORLD_LEFT || new_x > WORLD_RIGHT ||
                    new_y < WORLD_TOP || new_y > WORLD_BOTTOM) {
                    continue;
                }
                
                // Check if position is free
                if (is_position_free(new_x, new_y, pt->branch_distance * 0.8f)) {
                    int new_node = simulation_add_node(new_x, new_y, plant_type);
                    if (new_node >= 0) {
                        // Create connection between parent and child
                        simulation_add_chain(i, new_node);
                        nodes[i].branch_count++;
                        grown++;
                        
                        // FIXED: Always calculate and store nutrition cost for ALL plants
                        float nutrition_cost = calculate_plant_nutrition_cost(pt);
                        nodes[new_node].stored_nutrition = nutrition_cost;
                        
                        // SUBTRACT from environmental balance (plant consumed nutrition from ground)
                        g_environmental_nutrition_balance -= nutrition_cost;
                        
                        // Deplete from nutrition layer visually
                        nutrition_deplete_at_position(new_x, new_y, nutrition_cost, 
                                                    STANDARD_DEPLETION_RANGE);
                        
                        break; // Stop trying for this parent
                    }
                }
            }
        }
    }
}

// Add nutrition back to environment (when fish defecate)
void plants_add_environmental_nutrition(float amount) {
    g_environmental_nutrition_balance += amount;
}

// Accessor functions
int plants_get_type_count(void) {
    return g_plant_type_count;
}

PlantType* plants_get_type(int index) {
    if (index < 0 || index >= g_plant_type_count) {
        return NULL;
    }
    return &g_plant_types[index];
}

// Get stored nutrition from a plant node
float plants_get_nutrition_from_node(int node_id) {
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    
    if (node_id < 0 || node_id >= node_count) {
        return 0.0f;
    }
    
    if (!nodes[node_id].active || nodes[node_id].plant_type < 0) {
        return 0.0f;
    }
    
    return nodes[node_id].stored_nutrition;
}

// Get total environmental nutrition (balance from initial state)
float plants_get_total_environmental_nutrition(void) {
    calculate_initial_environmental_nutrition();
    return g_environmental_nutrition_balance;  // Return only the balance (starts at 0)
}

// FIXED: Initialize plant node with nutrition cost (for manually placed plants)
void plants_initialize_nutrition_cost(int node_id, int plant_type) {
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    
    if (node_id < 0 || node_id >= node_count) return;
    if (plant_type < 0 || plant_type >= g_plant_type_count) return;
    
    PlantType* pt = &g_plant_types[plant_type];
    
    // FIXED: Use same calculation for manually placed plants
    float nutrition_cost = calculate_plant_nutrition_cost(pt);
    
    nodes[node_id].stored_nutrition = nutrition_cost;
    
    // SUBTRACT from environmental balance (manually placed plant also consumes nutrition)
    calculate_initial_environmental_nutrition();
    g_environmental_nutrition_balance -= nutrition_cost;
    
    printf("Manual plant %s initialized with nutrition cost: %.3f\n", pt->name, nutrition_cost);
}

// Legacy compatibility functions
float plants_get_nutrition_cost_for_node(int node_id) {
    return plants_get_nutrition_from_node(node_id);
}

float plants_get_total_nutrition_cost(void) {
    return plants_get_total_environmental_nutrition();
}