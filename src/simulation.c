#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "simulation.h"
#include "grid.h"
#include "camera.h"

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
    
    printf("Simulation initialized\n");
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
    node->plant_type = plant_type;
    node->branch_count = 0;
    node->age = 0;
    
    return g_node_count++;
}

int simulation_add_chain(int node1, int node2) {
    if (g_chain_count >= MAX_CHAINS) {
        printf("Maximum chains reached\n");
        return -1;
    }
    if (node1 == node2) return -1;
    if (node1 < 0 || node1 >= g_node_count) return -1;
    if (node2 < 0 || node2 >= g_node_count) return -1;
    
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
    
    // Generate random curve parameters
    chain->curve_strength = ((float)rand() / RAND_MAX - 0.5f) * 0.6f;
    chain->curve_offset = ((float)rand() / RAND_MAX - 0.5f) * 20.0f;
    
    return g_chain_count++;
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