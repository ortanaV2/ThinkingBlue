#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "gas.h"
#include "camera.h"

#define GAS_GRID_SIZE 30.0f
#define GAS_DECAY_RATE 0.0008f
#define GAS_BASE_LEVEL 0.3f

static float* g_oxygen_grid = NULL;
static int g_grid_width = 0;
static int g_grid_height = 0;
static int g_visible = 0;
static SDL_Renderer* g_renderer = NULL;

int gas_init(void) {
    g_grid_width = (int)ceil(WORLD_WIDTH / GAS_GRID_SIZE);
    g_grid_height = (int)ceil(WORLD_HEIGHT / GAS_GRID_SIZE);
    
    g_oxygen_grid = malloc(g_grid_width * g_grid_height * sizeof(float));
    
    if (!g_oxygen_grid) {
        printf("Failed to allocate oxygen grid\n");
        return 0;
    }
    
    // Initialize with base oxygen level
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_oxygen_grid[i] = GAS_BASE_LEVEL;
    }
    
    g_visible = 0;
    
    printf("Gas layer initialized: %dx%d grid\n", g_grid_width, g_grid_height);
    return 1;
}

void gas_cleanup(void) {
    if (g_oxygen_grid) {
        free(g_oxygen_grid);
        g_oxygen_grid = NULL;
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
    *grid_x = (int)floor((world_x - WORLD_LEFT) / GAS_GRID_SIZE);
    *grid_y = (int)floor((world_y - WORLD_TOP) / GAS_GRID_SIZE);
}

float gas_get_oxygen_at(float world_x, float world_y) {
    int grid_x, grid_y;
    world_to_gas_grid(world_x, world_y, &grid_x, &grid_y);
    
    if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
        return GAS_BASE_LEVEL;
    }
    
    return g_oxygen_grid[grid_y * g_grid_width + grid_x];
}

void gas_produce_oxygen_at_position(float world_x, float world_y, float production_amount, float radius) {
    if (!g_oxygen_grid) return;
    
    int center_x = (int)floor((world_x - WORLD_LEFT) / GAS_GRID_SIZE);
    int center_y = (int)floor((world_y - WORLD_TOP) / GAS_GRID_SIZE);
    
    int grid_radius = (int)ceil(radius / GAS_GRID_SIZE);
    
    for (int dy = -grid_radius; dy <= grid_radius; dy++) {
        for (int dx = -grid_radius; dx <= grid_radius; dx++) {
            int grid_x = center_x + dx;
            int grid_y = center_y + dy;
            
            if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
                continue;
            }
            
            float distance = sqrt(dx * dx + dy * dy) * GAS_GRID_SIZE;
            
            if (distance <= radius) {
                float falloff = 1.0f - (distance / radius);
                falloff = falloff * falloff; // Quadratic falloff
                
                float actual_production = production_amount * falloff;
                
                int index = grid_y * g_grid_width + grid_x;
                g_oxygen_grid[index] += actual_production;
                
                // Cap at maximum oxygen level
                if (g_oxygen_grid[index] > 1.0f) {
                    g_oxygen_grid[index] = 1.0f;
                }
            }
        }
    }
}

void gas_decay_oxygen(void) {
    if (!g_oxygen_grid) return;
    
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_oxygen_grid[i] -= GAS_DECAY_RATE;
        
        // Don't go below zero
        if (g_oxygen_grid[i] < 0.0f) {
            g_oxygen_grid[i] = 0.0f;
        }
    }
}

static void value_to_oxygen_color(float value, int* r, int* g, int* b) {
    // Blue-to-cyan gradient for oxygen levels
    // 0.0 = dark blue (low oxygen), 1.0 = bright cyan (high oxygen)
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    
    if (value < 0.2f) {
        // Very low oxygen: dark blue to navy
        float t = value / 0.2f;
        *r = (int)(20 * t);
        *g = (int)(30 * t);
        *b = (int)(80 + 60 * t);
    } else if (value < 0.5f) {
        // Low to medium oxygen: navy to blue
        float t = (value - 0.2f) / 0.3f;
        *r = (int)(20 + 30 * t);
        *g = (int)(30 + 70 * t);
        *b = (int)(140 + 55 * t);
    } else if (value < 0.8f) {
        // Medium to high oxygen: blue to light blue
        float t = (value - 0.5f) / 0.3f;
        *r = (int)(50 + 80 * t);
        *g = (int)(100 + 100 * t);
        *b = (int)(195 + 35 * t);
    } else {
        // High oxygen: light blue to cyan
        float t = (value - 0.8f) / 0.2f;
        *r = (int)(130 + 95 * t);
        *g = (int)(200 + 55 * t);
        *b = (int)(230 + 25 * t);
    }
}

void gas_render(void) {
    if (!g_visible || !g_renderer || !g_oxygen_grid) return;
    
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    int start_x = (int)floor((world_left - WORLD_LEFT) / GAS_GRID_SIZE) - 1;
    int end_x = (int)ceil((world_right - WORLD_LEFT) / GAS_GRID_SIZE) + 1;
    int start_y = (int)floor((world_top - WORLD_TOP) / GAS_GRID_SIZE) - 1;
    int end_y = (int)ceil((world_bottom - WORLD_TOP) / GAS_GRID_SIZE) + 1;
    
    if (start_x < 0) start_x = 0;
    if (end_x >= g_grid_width) end_x = g_grid_width - 1;
    if (start_y < 0) start_y = 0;
    if (end_y >= g_grid_height) end_y = g_grid_height - 1;
    
    for (int gy = start_y; gy <= end_y; gy++) {
        for (int gx = start_x; gx <= end_x; gx++) {
            float oxygen_value = g_oxygen_grid[gy * g_grid_width + gx];
            
            int r, g, b;
            value_to_oxygen_color(oxygen_value, &r, &g, &b);
            
            SDL_SetRenderDrawColor(g_renderer, r, g, b, 120); // Semi-transparent
            
            float world_x = WORLD_LEFT + gx * GAS_GRID_SIZE;
            float world_y = WORLD_TOP + gy * GAS_GRID_SIZE;
            
            int screen_x1, screen_y1, screen_x2, screen_y2;
            camera_world_to_screen(world_x, world_y, &screen_x1, &screen_y1);
            camera_world_to_screen(world_x + GAS_GRID_SIZE, world_y + GAS_GRID_SIZE, 
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