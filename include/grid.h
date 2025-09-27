#ifndef GRID_H
#define GRID_H

#include "types.h"

// System initialization and cleanup
int grid_init(void);
void grid_cleanup(void);

// Grid management
void grid_clear(void);
void grid_add_node(int node_index);
void grid_rebuild(void);

// Grid queries
int grid_get_cells_at_position(float world_x, float world_y, GridCell** cells, int max_cells);
void grid_get_nearby_cells(int grid_x, int grid_y, int radius, GridCell** cells, int* cell_count, int max_cells);

// Coordinate conversion
void grid_world_to_grid_coords(float world_x, float world_y, int* grid_x, int* grid_y);

#endif // GRID_H