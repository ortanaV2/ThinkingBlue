#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"

// Core simulation management
int simulation_init(void);
void simulation_cleanup(void);

// Node and chain management
int simulation_add_node(float x, float y, int plant_type);
int simulation_add_chain(int node1, int node2);
int simulation_find_node_at_position(float world_x, float world_y);

// NEW: Seed immunity system
int simulation_add_seed_node(float x, float y, int plant_type);
void simulation_update_seed_timers(void);

// State accessors
Node* simulation_get_nodes(void);
Chain* simulation_get_chains(void);
int simulation_get_node_count(void);
int simulation_get_chain_count(void);

// Selection management
int simulation_get_selected_node(void);
int simulation_get_selection_mode(void);
void simulation_set_selected_node(int node_index);
void simulation_clear_selection(void);

// Frame counter
void simulation_update_frame_counter(void);
int simulation_get_frame_counter(void);

#endif // SIMULATION_H