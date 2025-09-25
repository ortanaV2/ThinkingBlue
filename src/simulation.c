// simulation.c - Enhanced with simplified nutrition system
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "simulation.h"
#include "grid.h"
#include "camera.h"
#include "plants.h"

static Node* g_nodes = NULL;
static Chain* g_chains = NULL;
static int g_node_count = 0;
static int g_chain_count = 0;
static int g_selected_node = -1;
static int g_selection_mode = 0;
static int g_frame_counter = 0;

int simulation_init(void) {
    // Allocate main arrays
    g_nodes = (Node*)calloc(MAX_NODES, sizeof(Node));
    g_chains = (Chain*)calloc(MAX_CHAINS, sizeof(Chain));
    
    if (!g_nodes || !g_chains) {
        printf("Failed to allocate memory for simulation\n");
        return 0;
    }
    
    // Initialize state
    g_node_count = 0;
    g_chain_count = 0;
    g_selected_node = -1;
    g_selection_mode = 0;
    g_frame_counter = 0;
    
    // Initialize grid system
    if (!grid_init()) {
        printf("Failed to initialize grid system\n");
        return 0;
    }
    
    printf("Simulation initialized with simplified nutrition system\n");
    return 1;
}

void simulation_cleanup(void) {
    if (g_nodes) {
        free(g_nodes);
        g_nodes = NULL;
    }
    if (g_chains) {
        free(g_chains);
        g_chains = NULL;
    }
    
    grid_cleanup();
}

int simulation_add_node(float x, float y, int plant_type) {
    if (g_node_count >= MAX_NODES) {
        printf("Maximum nodes reached\n");
        return -1;
    }
    
    // Clamp to world bounds
    if (x < WORLD_LEFT) x = WORLD_LEFT;
    if (x > WORLD_RIGHT) x = WORLD_RIGHT;
    if (y < WORLD_TOP) y = WORLD_TOP;
    if (y > WORLD_BOTTOM) y = WORLD_BOTTOM;
    
    Node* node = &g_nodes[g_node_count];
    node->x = x;
    node->y = y;
    node->vx = 0;
    node->vy = 0;
    node->active = 1;
    node->can_grow = 1;
    node->plant_type = plant_type;  // -1 for fish, -2 for corpse, >=0 for plants
    node->branch_count = 0;
    node->age = 0;
    
    // Initialize corpse system fields
    node->is_corpse = 0;
    node->corpse_decay_timer = 0;
    node->original_fish_type = -1;
    node->corpse_heading = 0.0f;
    
    // Initialize seed immunity system
    node->seed_immunity_timer = 0;
    
    // Initialize nutrition storage
    node->stored_nutrition = 0.0f;
    
    // Handle special node types
    if (plant_type == -2) {
        // Corpse node
        node->can_grow = 0;
    } else if (plant_type == -1) {
        // Fish node
        node->can_grow = 0;
    } else if (plant_type >= 0) {
        // Plant node - initialize nutrition cost for manually placed plants
        plants_initialize_nutrition_cost(g_node_count, plant_type);
    }
    
    return g_node_count++;
}

// Add node as seed with immunity
int simulation_add_seed_node(float x, float y, int plant_type) {
    int node_id = simulation_add_node(x, y, plant_type);
    if (node_id >= 0) {
        g_nodes[node_id].seed_immunity_timer = SEED_IMMUNITY_TIME;
        printf("Created immune seed %s at (%.0f, %.0f) - immune for %d frames\n",
               plants_get_type(plant_type) ? plants_get_type(plant_type)->name : "Unknown", 
               x, y, SEED_IMMUNITY_TIME);
    }
    return node_id;
}

int simulation_add_chain(int node1, int node2) {
    if (g_chain_count >= MAX_CHAINS) {
        printf("Maximum chains reached\n");
        return -1;
    }
    if (node1 == node2) return -1;
    if (node1 < 0 || node1 >= g_node_count) return -1;
    if (node2 < 0 || node2 >= g_node_count) return -1;
    
    // Don't create chains for fish nodes or corpses
    if (g_nodes[node1].plant_type < 0 || g_nodes[node2].plant_type < 0) return -1;
    if (g_nodes[node1].is_corpse || g_nodes[node2].is_corpse) return -1;
    
    // Check for duplicate chains in recent history
    int check_count = (g_chain_count > 1000) ? 1000 : g_chain_count;
    for (int i = g_chain_count - check_count; i < g_chain_count; i++) {
        if (i < 0) continue;
        if (!g_chains[i].active) continue;
        if ((g_chains[i].node1 == node1 && g_chains[i].node2 == node2) ||
            (g_chains[i].node1 == node2 && g_chains[i].node2 == node1)) {
            return -1;
        }
    }
    
    Chain* chain = &g_chains[g_chain_count];
    chain->node1 = node1;
    chain->node2 = node2;
    chain->active = 1;
    chain->plant_type = g_nodes[node1].plant_type;
    chain->age = 0;
    
    // Generate curve parameters based on plant type
    PlantType* plant_type = plants_get_type(g_nodes[node1].plant_type);
    float curvature_factor = plant_type ? plant_type->chain_curvature_factor : 1.0f;
    
    // Base curve strength and offset
    chain->curve_strength = ((float)rand() / RAND_MAX - 0.5f) * 0.6f;
    chain->curve_offset = ((float)rand() / RAND_MAX - 0.5f) * 20.0f;
    
    // Apply plant-specific curvature factor with additional randomness
    chain->curve_multiplier = curvature_factor * (0.8f + ((float)rand() / RAND_MAX) * 0.4f);
    
    return g_chain_count++;
}

// Update seed immunity timers
void simulation_update_seed_timers(void) {
    int seeds_matured = 0;
    
    for (int i = 0; i < g_node_count; i++) {
        if (!g_nodes[i].active) continue;
        if (g_nodes[i].plant_type < 0) continue;  // Skip fish and corpse nodes
        if (g_nodes[i].seed_immunity_timer <= 0) continue;  // Skip non-immune nodes
        
        g_nodes[i].seed_immunity_timer--;
        
        if (g_nodes[i].seed_immunity_timer == 0) {
            seeds_matured++;
        }
    }
    
    // Debug output for seed maturation
    static int last_seed_log = 0;
    if (seeds_matured > 0 && (g_frame_counter - last_seed_log) > 300) {
        printf("Seeds matured: %d seeds are now edible\n", seeds_matured);
        last_seed_log = g_frame_counter;
    }
}

int simulation_find_node_at_position(float world_x, float world_y) {
    float threshold = NODE_RADIUS * 2 / camera_get_zoom();
    
    // Use grid to find nearby nodes
    GridCell* cells[9];
    int cell_count = grid_get_cells_at_position(world_x, world_y, cells, 9);
    
    for (int c = 0; c < cell_count; c++) {
        GridCell* cell = cells[c];
        if (!cell) continue;
        
        for (int k = 0; k < cell->count; k++) {
            int i = cell->node_indices[k];
            if (i < 0 || i >= g_node_count) continue;
            if (!g_nodes[i].active) continue;
            
            float dx = g_nodes[i].x - world_x;
            float dy = g_nodes[i].y - world_y;
            float distance = dx * dx + dy * dy;
            
            if (distance <= threshold * threshold) {
                return i;
            }
        }
    }
    return -1;
}

void simulation_update_frame_counter(void) {
    g_frame_counter++;
    
    // Update seed immunity timers every frame
    simulation_update_seed_timers();
}

int simulation_get_frame_counter(void) {
    return g_frame_counter;
}

Node* simulation_get_nodes(void) {
    return g_nodes;
}

Chain* simulation_get_chains(void) {
    return g_chains;
}

int simulation_get_node_count(void) {
    return g_node_count;
}

int simulation_get_chain_count(void) {
    return g_chain_count;
}

int simulation_get_selected_node(void) {
    return g_selected_node;
}

int simulation_get_selection_mode(void) {
    return g_selection_mode;
}

void simulation_set_selected_node(int node_index) {
    g_selected_node = node_index;
    g_selection_mode = 1;
}

void simulation_clear_selection(void) {
    g_selected_node = -1;
    g_selection_mode = 0;
}