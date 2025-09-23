#include <math.h>

#include "types.h"
#include "physics.h"
#include "simulation.h"
#include "grid.h"
#include "plants.h"
#include "fish.h"

static void apply_repulsion_forces(void) {
    float optimal_sq = OPTIMAL_DISTANCE * OPTIMAL_DISTANCE;
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    
    for (int gx = 0; gx < (int)ceil(WORLD_WIDTH / GRID_SIZE); gx++) {
        for (int gy = 0; gy < (int)ceil(WORLD_HEIGHT / GRID_SIZE); gy++) {
            GridCell* cells[9];
            int cell_count = 0;
            grid_get_nearby_cells(gx, gy, 1, cells, &cell_count, 9);
            
            // Check pairs within same cell
            if (cell_count > 0) {
                GridCell* cell = cells[0]; // Center cell
                for (int a = 0; a < cell->count; a++) {
                    for (int b = a + 1; b < cell->count; b++) {
                        int i = cell->node_indices[a];
                        int j = cell->node_indices[b];
                        
                        if (i < 0 || i >= node_count || j < 0 || j >= node_count) continue;
                        if (!nodes[i].active || !nodes[j].active) continue;
                        
                        float dx = nodes[j].x - nodes[i].x;
                        float dy = nodes[j].y - nodes[i].y;
                        float distance_sq = dx * dx + dy * dy;
                        
                        if (distance_sq < optimal_sq && distance_sq > 0) {
                            float distance = sqrt(distance_sq);
                            float force_magnitude = REPULSION_FORCE * (OPTIMAL_DISTANCE - distance) / distance;
                            float fx = -dx * force_magnitude;
                            float fy = -dy * force_magnitude;
                            
                            // Apply mobility factor based on node type
                            float mobility_i = 1.0f;
                            float mobility_j = 1.0f;
                            
                            // Check if nodes are plants or fish
                            if (nodes[i].plant_type >= 0) {
                                // Plant node
                                PlantType* pt_i = plants_get_type(nodes[i].plant_type);
                                if (pt_i) mobility_i = pt_i->mobility_factor;
                            }
                            // Fish nodes use default mobility (1.0f)
                            
                            if (nodes[j].plant_type >= 0) {
                                // Plant node
                                PlantType* pt_j = plants_get_type(nodes[j].plant_type);
                                if (pt_j) mobility_j = pt_j->mobility_factor;
                            }
                            // Fish nodes use default mobility (1.0f)
                            
                            nodes[i].vx += fx * mobility_i;
                            nodes[i].vy += fy * mobility_i;
                            nodes[j].vx -= fx * mobility_j;
                            nodes[j].vy -= fy * mobility_j;
                        }
                    }
                }
            }
            
            // Check pairs with adjacent cells
            for (int c = 1; c < cell_count; c++) {
                GridCell* cell1 = cells[0];
                GridCell* cell2 = cells[c];
                
                for (int a = 0; a < cell1->count; a++) {
                    for (int b = 0; b < cell2->count; b++) {
                        int i = cell1->node_indices[a];
                        int j = cell2->node_indices[b];
                        
                        if (i < 0 || i >= node_count || j < 0 || j >= node_count) continue;
                        if (!nodes[i].active || !nodes[j].active) continue;
                        
                        float dx = nodes[j].x - nodes[i].x;
                        float dy = nodes[j].y - nodes[i].y;
                        float distance_sq = dx * dx + dy * dy;
                        
                        if (distance_sq < optimal_sq && distance_sq > 0) {
                            float distance = sqrt(distance_sq);
                            float base_force = REPULSION_FORCE * (OPTIMAL_DISTANCE - distance) / distance;
                            
                            float mobility_i = 1.0f;
                            float mobility_j = 1.0f;
                            
                            // Check if nodes are plants or fish
                            if (nodes[i].plant_type >= 0) {
                                PlantType* pt_i = plants_get_type(nodes[i].plant_type);
                                if (pt_i) mobility_i = pt_i->mobility_factor;
                            }
                            
                            if (nodes[j].plant_type >= 0) {
                                PlantType* pt_j = plants_get_type(nodes[j].plant_type);
                                if (pt_j) mobility_j = pt_j->mobility_factor;
                            }
                            
                            // Static plants create stronger repulsion
                            float repulsion_strength_i = 2.0f - mobility_i;
                            float repulsion_strength_j = 2.0f - mobility_j;
                            
                            float fx = -dx * base_force * repulsion_strength_j;
                            float fy = -dy * base_force * repulsion_strength_j;
                            float fx2 = dx * base_force * repulsion_strength_i;
                            float fy2 = dy * base_force * repulsion_strength_i;
                            
                            nodes[i].vx += fx * mobility_i;
                            nodes[i].vy += fy * mobility_i;
                            nodes[j].vx += fx2 * mobility_j;
                            nodes[j].vy += fy2 * mobility_j;
                        }
                    }
                }
            }
        }
    }
}

static void apply_chain_forces(void) {
    Chain* chains = simulation_get_chains();
    Node* nodes = simulation_get_nodes();
    int chain_count = simulation_get_chain_count();
    int node_count = simulation_get_node_count();
    
    for (int i = 0; i < chain_count; i++) {
        if (!chains[i].active) continue;
        
        int n1 = chains[i].node1;
        int n2 = chains[i].node2;
        
        if (n1 < 0 || n1 >= node_count || n2 < 0 || n2 >= node_count) continue;
        if (!nodes[n1].active || !nodes[n2].active) continue;
        
        // Skip chains involving fish nodes (fish don't have chains)
        if (nodes[n1].plant_type == -1 || nodes[n2].plant_type == -1) continue;
        
        float dx = nodes[n2].x - nodes[n1].x;
        float dy = nodes[n2].y - nodes[n1].y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > 0) {
            float force_magnitude = CHAIN_FORCE * (distance - OPTIMAL_DISTANCE) / distance;
            float fx = dx * force_magnitude;
            float fy = dy * force_magnitude;
            
            // Apply mobility factor for chain forces (only for plants)
            PlantType* pt_n1 = plants_get_type(nodes[n1].plant_type);
            PlantType* pt_n2 = plants_get_type(nodes[n2].plant_type);
            
            float mobility_n1 = pt_n1 ? pt_n1->mobility_factor : 1.0f;
            float mobility_n2 = pt_n2 ? pt_n2->mobility_factor : 1.0f;
            
            nodes[n1].vx += fx * mobility_n1;
            nodes[n1].vy += fy * mobility_n1;
            nodes[n2].vx -= fx * mobility_n2;
            nodes[n2].vy -= fy * mobility_n2;
        }
    }
}

// Helper function to turn fish 180 degrees and face inward
static void turn_fish_180_inward(int node_id) {
    Fish* all_fish = fish_get_all();
    int fish_count = fish_get_count();
    
    // Find the fish that owns this node
    for (int i = 0; i < fish_count; i++) {
        if (all_fish[i].active && all_fish[i].node_id == node_id) {
            // Turn fish 180 degrees
            all_fish[i].heading += M_PI;
            
            // Normalize heading to [0, 2π]
            while (all_fish[i].heading >= 2.0f * M_PI) {
                all_fish[i].heading -= 2.0f * M_PI;
            }
            while (all_fish[i].heading < 0.0f) {
                all_fish[i].heading += 2.0f * M_PI;
            }
            
            break;
        }
    }
}

void physics_update(void) {
    simulation_update_frame_counter();
    
    // Rebuild spatial grid periodically
    if (simulation_get_frame_counter() % 5 == 0) {
        grid_rebuild();
    }
    
    // Apply forces
    apply_repulsion_forces();
    apply_chain_forces();
    
    // Plant growth (includes gas heatmap update)
    plants_grow();
    
    // Update positions with drag and world bounds
    Node* nodes = simulation_get_nodes();
    Chain* chains = simulation_get_chains();
    int node_count = simulation_get_node_count();
    int chain_count = simulation_get_chain_count();
    
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        
        // Age nodes
        nodes[i].age++;
        
        // Apply drag
        nodes[i].vx *= WATER_DRAG;
        nodes[i].vy *= WATER_DRAG;
        
        // Update position
        nodes[i].x += nodes[i].vx;
        nodes[i].y += nodes[i].vy;
        
        // Enhanced world bounds collision for fish - turn 180° and face inward
        if (nodes[i].plant_type == -1) { // Fish node
            int collision_occurred = 0;
            
            if (nodes[i].x < WORLD_LEFT) {
                nodes[i].x = WORLD_LEFT;
                nodes[i].vx = 0;
                collision_occurred = 1;
            }
            if (nodes[i].x > WORLD_RIGHT) {
                nodes[i].x = WORLD_RIGHT;
                nodes[i].vx = 0;
                collision_occurred = 1;
            }
            if (nodes[i].y < WORLD_TOP) {
                nodes[i].y = WORLD_TOP;
                nodes[i].vy = 0;
                collision_occurred = 1;
            }
            if (nodes[i].y > WORLD_BOTTOM) {
                nodes[i].y = WORLD_BOTTOM;
                nodes[i].vy = 0;
                collision_occurred = 1;
            }
            
            // If collision occurred, turn fish 180° to face inward
            if (collision_occurred) {
                turn_fish_180_inward(i);
            }
        } else {
            // Standard collision for plants and corpses
            if (nodes[i].x < WORLD_LEFT) {
                nodes[i].x = WORLD_LEFT;
                nodes[i].vx = 0;
            }
            if (nodes[i].x > WORLD_RIGHT) {
                nodes[i].x = WORLD_RIGHT;
                nodes[i].vx = 0;
            }
            if (nodes[i].y < WORLD_TOP) {
                nodes[i].y = WORLD_TOP;
                nodes[i].vy = 0;
            }
            if (nodes[i].y > WORLD_BOTTOM) {
                nodes[i].y = WORLD_BOTTOM;
                nodes[i].vy = 0;
            }
        }
    }
    
    // Age chains
    for (int i = 0; i < chain_count; i++) {
        if (chains[i].active) {
            chains[i].age++;
        }
    }
}