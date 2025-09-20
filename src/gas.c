#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "gas.h"
#include "camera.h"
#include "simulation.h"
#include "plants.h"
#include "temperature.h"

#define GAS_DECAY_RATE 0.002f
#define GAS_BASE_LEVEL 0.0f

static float* g_oxygen_grid = NULL;
static float* g_oxygen_target = NULL;
static int g_grid_width = 0;
static int g_grid_height = 0;
static int g_visible = 0;
static SDL_Renderer* g_renderer = NULL;

int gas_init(void) {
    g_grid_width = (int)ceil(WORLD_WIDTH / LAYER_GRID_SIZE);
    g_grid_height = (int)ceil(WORLD_HEIGHT / LAYER_GRID_SIZE);
    
    g_oxygen_grid = malloc(g_grid_width * g_grid_height * sizeof(float));
    g_oxygen_target = malloc(g_grid_width * g_grid_height * sizeof(float));
    
    if (!g_oxygen_grid || !g_oxygen_target) {
        printf("Failed to allocate oxygen grids\n");
        return 0;
    }
    
    // Initialize both grids with base oxygen level
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_oxygen_grid[i] = GAS_BASE_LEVEL;
        g_oxygen_target[i] = GAS_BASE_LEVEL;
    }
    
    g_visible = 0;
    
    printf("Gas layer initialized: %dx%d grid (%.1f unit cells)\n", 
           g_grid_width, g_grid_height, LAYER_GRID_SIZE);
    return 1;
}

void gas_cleanup(void) {
    if (g_oxygen_grid) {
        free(g_oxygen_grid);
        g_oxygen_grid = NULL;
    }
    if (g_oxygen_target) {
        free(g_oxygen_target);
        g_oxygen_target = NULL;
    }
}

void gas_set_renderer(SDL_Renderer* renderer) {
    g_renderer = renderer;
}

void gas_toggle_visibility(void) {
    g_visible = !g_visible;
    printf("Gas layer: %s\n", g_visible ? "ON" : "OFF");
}

int gas_is_visible(void) {
    return g_visible;
}

static void world_to_gas_grid(float world_x, float world_y, int* grid_x, int* grid_y) {
    *grid_x = (int)floor((world_x - WORLD_LEFT) / LAYER_GRID_SIZE);
    *grid_y = (int)floor((world_y - WORLD_TOP) / LAYER_GRID_SIZE);
}

float gas_get_oxygen_at(float world_x, float world_y) {
    int grid_x, grid_y;
    world_to_gas_grid(world_x, world_y, &grid_x, &grid_y);
    
    if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
        return GAS_BASE_LEVEL;
    }
    
    return g_oxygen_grid[grid_y * g_grid_width + grid_x];
}

void gas_decay_oxygen(void) {
    if (!g_oxygen_grid) return;
    
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_oxygen_grid[i] -= GAS_DECAY_RATE;
        
        if (g_oxygen_grid[i] < 0.0f) {
            g_oxygen_grid[i] = 0.0f;
        }
    }
}

void gas_update_heatmap(void) {
    if (!g_oxygen_grid || !g_oxygen_target) return;
    
    // Clear target grid
    memset(g_oxygen_target, 0, g_grid_width * g_grid_height * sizeof(float));
    
    // Calculate target oxygen levels from current plant positions
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    
    for (int node_idx = 0; node_idx < node_count; node_idx++) {
        if (!nodes[node_idx].active) continue;
        
        int plant_type = nodes[node_idx].plant_type;
        if (plant_type < 0 || plant_type >= plants_get_type_count()) continue;
        
        PlantType* pt = plants_get_type(plant_type);
        if (!pt) continue;
        
        // Check if coral is bleached - bleached corals produce no oxygen
        if (temperature_is_coral_bleached(node_idx)) {
            continue;  // Skip oxygen production for bleached corals
        }
        
        float node_x = nodes[node_idx].x;
        float node_y = nodes[node_idx].y;
        float production_factor = pt->oxygen_production_factor;
        float production_radius = pt->oxygen_production_radius;
        
        if (production_factor <= 0.0f || production_radius <= 0.0f) continue;
        
        // Calculate grid bounds for this node's influence
        int center_grid_x = (int)floor((node_x - WORLD_LEFT) / LAYER_GRID_SIZE);
        int center_grid_y = (int)floor((node_y - WORLD_TOP) / LAYER_GRID_SIZE);
        
        int grid_radius = (int)ceil(production_radius / LAYER_GRID_SIZE);
        
        // Apply oxygen production to target grid
        for (int dy = -grid_radius; dy <= grid_radius; dy++) {
            for (int dx = -grid_radius; dx <= grid_radius; dx++) {
                int grid_x = center_grid_x + dx;
                int grid_y = center_grid_y + dy;
                
                if (grid_x < 0 || grid_x >= g_grid_width || 
                    grid_y < 0 || grid_y >= g_grid_height) {
                    continue;
                }
                
                // Calculate world position of this grid cell center
                float grid_world_x = WORLD_LEFT + (grid_x + 0.5f) * LAYER_GRID_SIZE;
                float grid_world_y = WORLD_TOP + (grid_y + 0.5f) * LAYER_GRID_SIZE;
                
                // Calculate distance from node to grid cell
                float distance = sqrt((grid_world_x - node_x) * (grid_world_x - node_x) + 
                                    (grid_world_y - node_y) * (grid_world_y - node_y));
                
                if (distance <= production_radius) {
                    // Calculate normalized distance (0.0 at center, 1.0 at edge)
                    float normalized_distance = distance / production_radius;
                    
                    // Aggressive falloff
                    float falloff;
                    
                    if (normalized_distance < 0.3f) {
                        // Inner 30%: full strength with slight dropoff
                        falloff = 1.0f - (normalized_distance / 0.3f) * 0.2f; // 100% to 80%
                    } else if (normalized_distance < 0.6f) {
                        // Middle 30%: rapid dropoff
                        float t = (normalized_distance - 0.3f) / 0.3f;
                        falloff = 0.8f - t * t * t * 0.7f; // 80% to 10%
                    } else {
                        // Outer 40%: very steep dropoff to near zero
                        float t = (normalized_distance - 0.6f) / 0.4f;
                        falloff = 0.1f * (1.0f - t * t * t * t); // 10% to ~0%
                    }
                    
                    // Set target oxygen level (take maximum if multiple plants overlap)
                    float target_contribution = production_factor * falloff;
                    
                    int index = grid_y * g_grid_width + grid_x;
                    if (target_contribution > g_oxygen_target[index]) {
                        g_oxygen_target[index] = target_contribution;
                    }
                }
            }
        }
    }
    
    // Blend current oxygen levels towards target values
    float blend_rate = 0.08f;
    
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        float current = g_oxygen_grid[i];
        float target = g_oxygen_target[i];
        
        if (target > current) {
            // Oxygen increasing: blend towards target quickly
            g_oxygen_grid[i] = current + (target - current) * blend_rate * 0.5f;
        } else {
            // Oxygen decreasing: decay slowly for trail effect
            g_oxygen_grid[i] = current + (target - current) * blend_rate * 0.05f;
            
            // Additional natural decay
            g_oxygen_grid[i] -= GAS_DECAY_RATE;
        }
        
        // Clamp values
        if (g_oxygen_grid[i] < 0.0f) {
            g_oxygen_grid[i] = 0.0f;
        }
        if (g_oxygen_grid[i] > 2.0f) {
            g_oxygen_grid[i] = 2.0f;
        }
    }
}

static void value_to_oxygen_color(float value, int* r, int* g, int* b) {
    // Heatmap: black -> dark blue -> blue -> cyan -> yellow -> red
    if (value < 0.0f) value = 0.0f;
    if (value > 2.0f) value = 2.0f;
    
    if (value < 0.05f) {
        // Almost nothing: pure black to very dark blue
        float t = value / 0.05f;
        *r = 0;
        *g = 0;
        *b = (int)(20 * t);
    } else if (value < 0.15f) {
        // Very low: dark blue
        float t = (value - 0.05f) / 0.1f;
        *r = 0;
        *g = 0;
        *b = (int)(20 + 60 * t);
    } else if (value < 0.3f) {
        // Low: dark blue to medium blue
        float t = (value - 0.15f) / 0.15f;
        *r = 0;
        *g = (int)(30 * t);
        *b = (int)(80 + 80 * t);
    } else if (value < 0.5f) {
        // Medium: blue to cyan
        float t = (value - 0.3f) / 0.2f;
        *r = (int)(60 * t);
        *g = (int)(30 + 120 * t);
        *b = (int)(160 + 60 * t);
    } else if (value < 0.8f) {
        // High: cyan to yellow
        float t = (value - 0.5f) / 0.3f;
        *r = (int)(60 + 195 * t);
        *g = (int)(150 + 105 * t);
        *b = (int)(220 * (1.0f - t));
    } else {
        // Very high: yellow to bright red
        float t = (value - 0.8f) / 1.2f; // Scale for values up to 2.0
        if (t > 1.0f) t = 1.0f;
        *r = 255;
        *g = (int)(255 * (1.0f - t * 0.8f)); // Keep some yellow
        *b = 0;
    }
}

void gas_render(void) {
    if (!g_visible || !g_renderer || !g_oxygen_grid) return;
    
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    int start_x = (int)floor((world_left - WORLD_LEFT) / LAYER_GRID_SIZE) - 1;
    int end_x = (int)ceil((world_right - WORLD_LEFT) / LAYER_GRID_SIZE) + 1;
    int start_y = (int)floor((world_top - WORLD_TOP) / LAYER_GRID_SIZE) - 1;
    int end_y = (int)ceil((world_bottom - WORLD_TOP) / LAYER_GRID_SIZE) + 1;
    
    if (start_x < 0) start_x = 0;
    if (end_x >= g_grid_width) end_x = g_grid_width - 1;
    if (start_y < 0) start_y = 0;
    if (end_y >= g_grid_height) end_y = g_grid_height - 1;
    
    for (int gy = start_y; gy <= end_y; gy++) {
        for (int gx = start_x; gx <= end_x; gx++) {
            float oxygen_value = g_oxygen_grid[gy * g_grid_width + gx];
            
            // Skip rendering cells with very low oxygen
            if (oxygen_value < 0.02f) continue;
            
            int r, g, b;
            value_to_oxygen_color(oxygen_value, &r, &g, &b);
            
            // Dynamic alpha based on oxygen concentration
            int alpha = (int)(120 * fminf(oxygen_value / 1.0f, 1.0f));
            if (alpha < 30) alpha = 30; // Minimum visibility
            
            SDL_SetRenderDrawColor(g_renderer, r, g, b, alpha);
            
            float world_x = WORLD_LEFT + gx * LAYER_GRID_SIZE;
            float world_y = WORLD_TOP + gy * LAYER_GRID_SIZE;
            
            int screen_x1, screen_y1, screen_x2, screen_y2;
            camera_world_to_screen(world_x, world_y, &screen_x1, &screen_y1);
            camera_world_to_screen(world_x + LAYER_GRID_SIZE, world_y + LAYER_GRID_SIZE, 
                                 &screen_x2, &screen_y2);
            
            SDL_Rect rect;
            rect.x = screen_x1;
            rect.y = screen_y1;
            rect.w = screen_x2 - screen_x1;
            rect.h = screen_y2 - screen_y1;
            
            if (rect.w > 0 && rect.h > 0 && 
                rect.x < WINDOW_WIDTH && rect.y < WINDOW_HEIGHT &&
                rect.x + rect.w > 0 && rect.y + rect.h > 0) {
                SDL_RenderFillRect(g_renderer, &rect);
            }
        }
    }
}