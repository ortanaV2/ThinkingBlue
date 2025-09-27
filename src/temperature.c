#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include "temperature.h"
#include "simulation.h"
#include "plants.h"

static float g_current_temperature = 0.0f;  // Temperature offset in degrees Celsius
static int* g_bleached_nodes = NULL;        // Tracks bleached coral nodes
static int g_max_nodes = 0;

int temperature_init(void) {
    g_current_temperature = 0.0f;
    g_max_nodes = MAX_NODES;
    
    g_bleached_nodes = (int*)calloc(g_max_nodes, sizeof(int));
    if (!g_bleached_nodes) {
        printf("Failed to allocate bleached nodes tracking\n");
        return 0;
    }
    
    printf("Temperature system initialized (0.0°C - no bleaching)\n");
    return 1;
}

void temperature_cleanup(void) {
    if (g_bleached_nodes) {
        free(g_bleached_nodes);
        g_bleached_nodes = NULL;
    }
}

float temperature_get_current(void) {
    return g_current_temperature;
}

void temperature_set_current(float temp) {
    // Clamp temperature to valid range
    if (temp < 0.0f) temp = 0.0f;
    if (temp > 3.0f) temp = 3.0f;
    
    g_current_temperature = temp;
}

int temperature_is_coral_bleached(int node_id) {
    if (node_id < 0 || node_id >= g_max_nodes || !g_bleached_nodes) {
        return 0;
    }
    return g_bleached_nodes[node_id];
}

void temperature_process_coral_bleaching(void) {
    if (g_current_temperature <= 0.0f) {
        return;  // No bleaching at baseline temperature
    }
    
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    
    // Temperature-based bleaching probability
    float base_probability;
    if (g_current_temperature < 0.5f) {
        // Very low temperatures: minimal bleaching
        base_probability = g_current_temperature * 0.0001f / 100.0f;
    } else if (g_current_temperature < 1.0f) {
        // Low temperatures: rare bleaching
        float temp_factor = (g_current_temperature - 0.5f) / 0.5f;
        base_probability = (0.0001f + temp_factor * 0.009f) / 100.0f;
    } else if (g_current_temperature < 2.0f) {
        // Medium temperatures: uncommon bleaching
        float temp_factor = (g_current_temperature - 1.0f) / 1.0f;
        base_probability = (0.01f + temp_factor * 0.04f) / 100.0f;
    } else {
        // High temperatures: frequent bleaching
        float temp_factor = (g_current_temperature - 2.0f) / 1.0f;
        base_probability = (0.05f + temp_factor * 0.1f) / 100.0f;
    }
    
    // Limit to maximum 1 bleaching event per frame
    int bleaching_attempts = 0;
    int max_attempts_per_frame = 1;
    
    // Check only subset of nodes per frame for performance
    int nodes_to_check = node_count / 20;
    if (nodes_to_check < 1) nodes_to_check = 1;
    if (nodes_to_check > 50) nodes_to_check = 50;
    
    for (int attempt = 0; attempt < nodes_to_check && bleaching_attempts < max_attempts_per_frame; attempt++) {
        int i = rand() % node_count;
        
        if (!nodes[i].active) continue;
        if (nodes[i].plant_type == -1) continue;  // Skip fish nodes
        if (g_bleached_nodes[i]) continue;       // Already bleached
        
        // Only coral species can bleach
        PlantType* plant_type = plants_get_type(nodes[i].plant_type);
        if (!plant_type) continue;
        
        int is_coral = 0;
        if (strstr(plant_type->name, "Coral") != NULL) {
            is_coral = 1;
        }
        
        if (!is_coral) continue;
        
        // Age factor - older corals more susceptible
        float age_factor = 1.0f;
        if (nodes[i].age > plant_type->age_mature) {
            age_factor = 1.5f;
        }
        
        // Random bleaching check
        float random_val = (float)rand() / RAND_MAX;
        float adjusted_probability = base_probability * age_factor;
        
        if (random_val < adjusted_probability) {
            g_bleached_nodes[i] = 1;
            bleaching_attempts++;
            
            printf("Coral bleaching event: Node %d at temperature %.1f°C\n", 
                   i, g_current_temperature);
            
            // Bleach directly connected coral chains
            Chain* chains = simulation_get_chains();
            int chain_count = simulation_get_chain_count();
            int chains_bleached = 0;
            
            for (int c = 0; c < chain_count && chains_bleached < 3; c++) {
                if (!chains[c].active) continue;
                
                // Only bleach chains directly connected to this node
                if ((chains[c].node1 == i || chains[c].node2 == i) && chains[c].plant_type == nodes[i].plant_type) {
                    int connected_node = (chains[c].node1 == i) ? chains[c].node2 : chains[c].node1;
                    
                    if (connected_node < g_max_nodes && connected_node >= 0) {
                        if (nodes[connected_node].active && nodes[connected_node].plant_type >= 0) {
                            PlantType* connected_type = plants_get_type(nodes[connected_node].plant_type);
                            if (connected_type && strstr(connected_type->name, "Coral") != NULL) {
                                if (!g_bleached_nodes[connected_node]) {
                                    g_bleached_nodes[connected_node] = 1;
                                    chains_bleached++;
                                }
                            }
                        }
                    }
                }
            }
            
            if (chains_bleached > 0) {
                printf("  -> %d connected coral nodes also bleached\n", chains_bleached);
            }
        }
    }
}

void temperature_write_to_stats_file(FILE* stats_file) {
    if (!stats_file) return;
    
    fwrite(&g_current_temperature, sizeof(float), 1, stats_file);
}