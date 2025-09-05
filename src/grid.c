#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include "grid.h"
#include "simulation.h"

static GridCell* g_grid = NULL;
static int g_grid_width = 0;
static int g_grid_height = 0;

int grid_init(void) {
    // Calculate grid dimensions
    g_grid_width = (int)ceil(WORLD_WIDTH / GRID_SIZE);
    g_grid_height = (int)ceil(WORLD_HEIGHT / GRID_SIZE);
    
    // Allocate grid
    g_grid = (GridCell*)calloc(g_grid_width * g_grid_height, sizeof(GridCell));
    
    if (!g_grid) {
        printf("Failed to allocate spatial grid\n");
        return 0;
    }
    
    printf("Grid initialized: %dx%d cells\n", g_grid_width, g_grid_height);
    return 1;
}

void grid_cleanup(void) {
    if (g_grid) {
        free(g_grid);
        g_grid = NULL;
    }
}

static GridCell* get_grid_cell(int grid_x, int grid_y) {
    if (grid_x < 0 || grid_x >= g_grid_width || 
        grid_y < 0 || grid_y >= g_grid_height) {
        return NULL;
    }
    return &g_grid[grid_y * g_grid_width + grid_x];
}

static void world_to_grid(float world_x, float world_y, int* grid_x, int* grid_y) {
    *grid_x = (int)floor((world_x - WORLD_LEFT) / GRID_SIZE);
    *grid_y = (int)floor((world_y - WORLD_TOP) / GRID_SIZE);
}

void grid_clear(void) {
    if (!g_grid) return;
    
    int total_cells = g_grid_width * g_grid_height;
    for (int i = 0; i < total_cells; i++) {
        g_grid[i].count = 0;
    }
}

void grid_add_node(int node_index) {
    Node* nodes = simulation_get_nodes();
    if (node_index < 0 || node_index >= simulation_get_node_count()) return;
    if (!nodes[node_index].active) return;
    
    int grid_x, grid_y;
    world_to_grid(nodes[node_index].x, nodes[node_index].y, &grid_x, &grid_y);
    
    GridCell* cell = get_grid_cell(grid_x, grid_y);
    if (cell && cell->count < MAX_NODES_PER_CELL) {
        cell->node_indices[cell->count++] = node_index;
    }
}

void grid_rebuild(void) {
    grid_clear();
    
    int node_count = simulation_get_node_count();
    Node* nodes = simulation_get_nodes();
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active) {
            grid_add_node(i);
        }
    }
}

int grid_get_cells_at_position(float world_x, float world_y, GridCell** cells, int max_cells) {
    int grid_x, grid_y;
    world_to_grid(world_x, world_y, &grid_x, &grid_y);
    
    int count = 0;
    
    // Get 3x3 area around position
    for (int gx = grid_x - 1; gx <= grid_x + 1 && count < max_cells; gx++) {
        for (int gy = grid_y - 1; gy <= grid_y + 1 && count < max_cells; gy++) {
            GridCell* cell = get_grid_cell(gx, gy);
            if (cell) {
                cells[count++] = cell;
            }
        }
    }
    
    return count;
}

void grid_get_nearby_cells(int grid_x, int grid_y, int radius, GridCell** cells, int* cell_count, int max_cells) {
    *cell_count = 0;
    
    for (int gx = grid_x - radius; gx <= grid_x + radius && *cell_count < max_cells; gx++) {
        for (int gy = grid_y - radius; gy <= grid_y + radius && *cell_count < max_cells; gy++) {
            GridCell* cell = get_grid_cell(gx, gy);
            if (cell) {
                cells[(*cell_count)++] = cell;
            }
        }
    }
}

void grid_world_to_grid_coords(float world_x, float world_y, int* grid_x, int* grid_y) {
    world_to_grid(world_x, world_y, grid_x, grid_y);
}