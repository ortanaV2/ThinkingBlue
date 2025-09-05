#include <SDL2/SDL.h>
#include <math.h>

#include "types.h"
#include "rendering.h"
#include "simulation.h"
#include "camera.h"
#include "plants.h"

static SDL_Renderer* g_renderer = NULL;

static void calculate_aged_color(int base_r, int base_g, int base_b, int age, int* aged_r, int* aged_g, int* aged_b) {
    // Age factor: 0.0 (young) to 0.5 (old, max 50% aging)
    float age_factor = fminf((float)age / 3600.0f, 0.5f); // 1 minute at 60fps = fully aged
    
    // Darker but not brown target - keep some of original hue
    int dark_r = base_r * 0.3f;
    int dark_g = base_g * 0.4f; 
    int dark_b = base_b * 0.3f;
    
    // Interpolate between base color and darker version
    float inv_age = 1.0f - age_factor;
    float aged_float_r = base_r * inv_age + dark_r * age_factor;
    float aged_float_g = base_g * inv_age + dark_g * age_factor;
    float aged_float_b = base_b * inv_age + dark_b * age_factor;
    
    *aged_r = (int)(aged_float_r > 0 ? aged_float_r : 0);
    *aged_g = (int)(aged_float_g > 0 ? aged_float_g : 0);
    *aged_b = (int)(aged_float_b > 0 ? aged_float_b : 0);
}

static void draw_thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness) {
    if (thickness <= 1) {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    
    for (int i = -thickness/2; i <= thickness/2; i++) {
        SDL_RenderDrawLine(renderer, x1 + i, y1, x2 + i, y2);
        SDL_RenderDrawLine(renderer, x1, y1 + i, x2, y2 + i);
    }
}

static void draw_curved_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, float curve_strength, float curve_offset, int thickness) {
    // Calculate midpoint and perpendicular direction
    float mid_x = (x1 + x2) * 0.5f;
    float mid_y = (y1 + y2) * 0.5f;
    
    // Direction vector
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx * dx + dy * dy);
    
    if (length < 1.0f) {
        // Too short for curve, draw straight line
        draw_thick_line(renderer, x1, y1, x2, y2, thickness);
        return;
    }
    
    // Normalize direction
    dx /= length;
    dy /= length;
    
    // Perpendicular vector (rotated 90 degrees)
    float perp_x = -dy;
    float perp_y = dx;
    
    // Control point for quadratic curve
    float curve_amount = curve_strength * length * 0.3f + curve_offset;
    float ctrl_x = mid_x + perp_x * curve_amount;
    float ctrl_y = mid_y + perp_y * curve_amount;
    
    // Draw curve using multiple line segments
    int segments = (int)(length / 8.0f) + 3;
    if (segments > 20) segments = 20;
    
    float prev_x = x1;
    float prev_y = y1;
    
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / segments;
        float inv_t = 1.0f - t;
        
        // Quadratic bezier formula: (1-t)²P0 + 2(1-t)tP1 + t²P2
        float curr_x = inv_t * inv_t * x1 + 2 * inv_t * t * ctrl_x + t * t * x2;
        float curr_y = inv_t * inv_t * y1 + 2 * inv_t * t * ctrl_y + t * t * y2;
        
        draw_thick_line(renderer, (int)prev_x, (int)prev_y, (int)curr_x, (int)curr_y, thickness);
        
        prev_x = curr_x;
        prev_y = curr_y;
    }
}

int rendering_init(SDL_Renderer* renderer) {
    g_renderer = renderer;
    return 1;
}

void rendering_cleanup(void) {
    g_renderer = NULL;
}

void rendering_render(void) {
    if (!g_renderer) return;
    
    // Clear background
    SDL_SetRenderDrawColor(g_renderer, 30, 60, 120, 255);
    SDL_RenderClear(g_renderer);
    
    // Calculate viewport bounds for culling
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    Node* nodes = simulation_get_nodes();
    Chain* chains = simulation_get_chains();
    int node_count = simulation_get_node_count();
    int chain_count = simulation_get_chain_count();
    int selected_node = simulation_get_selected_node();
    int selection_mode = simulation_get_selection_mode();
    
    // Render chains
    for (int i = 0; i < chain_count; i++) {
        if (!chains[i].active) continue;
        
        int n1 = chains[i].node1;
        int n2 = chains[i].node2;
        
        if (n1 < 0 || n1 >= node_count || n2 < 0 || n2 >= node_count) continue;
        if (!nodes[n1].active || !nodes[n2].active) continue;
        
        // Frustum culling
        float min_x = fminf(nodes[n1].x, nodes[n2].x);
        float max_x = fmaxf(nodes[n1].x, nodes[n2].x);
        float min_y = fminf(nodes[n1].y, nodes[n2].y);
        float max_y = fmaxf(nodes[n1].y, nodes[n2].y);
        
        if (max_x < world_left || min_x > world_right || 
            max_y < world_top || min_y > world_bottom) {
            continue;
        }
        
        // Set chain color based on plant type and age
        int plant_type = chains[i].plant_type;
        PlantType* pt = plants_get_type(plant_type);
        if (pt) {
            int aged_r, aged_g, aged_b;
            calculate_aged_color(pt->chain_r, pt->chain_g, pt->chain_b, chains[i].age, &aged_r, &aged_g, &aged_b);
            SDL_SetRenderDrawColor(g_renderer, aged_r, aged_g, aged_b, 255);
        } else {
            SDL_SetRenderDrawColor(g_renderer, 100, 200, 100, 255);
        }
        
        int screen_x1, screen_y1, screen_x2, screen_y2;
        camera_world_to_screen(nodes[n1].x, nodes[n1].y, &screen_x1, &screen_y1);
        camera_world_to_screen(nodes[n2].x, nodes[n2].y, &screen_x2, &screen_y2);
        
        int thickness = (int)(CHAIN_THICKNESS * camera_get_zoom());
        if (thickness < 2) thickness = 2;
        
        draw_curved_line(g_renderer, screen_x1, screen_y1, screen_x2, screen_y2, 
                        chains[i].curve_strength, chains[i].curve_offset, thickness);
    }
    
    // Render nodes
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].active) continue;
        
        // Frustum culling
        if (nodes[i].x < world_left - NODE_RADIUS || nodes[i].x > world_right + NODE_RADIUS ||
            nodes[i].y < world_top - NODE_RADIUS || nodes[i].y > world_bottom + NODE_RADIUS) {
            continue;
        }
        
        int screen_x, screen_y;
        camera_world_to_screen(nodes[i].x, nodes[i].y, &screen_x, &screen_y);
        
        int scaled_radius = (int)(NODE_RADIUS * camera_get_zoom());
        if (scaled_radius < 1) scaled_radius = 1;
        
        // Set color based on selection state, plant type and age
        if (i == selected_node && selection_mode == 1) {
            SDL_SetRenderDrawColor(g_renderer, 255, 255, 0, 255);
        } else {
            int plant_type = nodes[i].plant_type;
            PlantType* pt = plants_get_type(plant_type);
            if (pt) {
                int aged_r, aged_g, aged_b;
                calculate_aged_color(pt->node_r, pt->node_g, pt->node_b, nodes[i].age, &aged_r, &aged_g, &aged_b);
                SDL_SetRenderDrawColor(g_renderer, aged_r, aged_g, aged_b, 255);
            } else {
                SDL_SetRenderDrawColor(g_renderer, 150, 255, 150, 255);
            }
        }
        
        // Draw node as circle
        if (scaled_radius <= 2) {
            SDL_RenderDrawPoint(g_renderer, screen_x, screen_y);
            if (scaled_radius > 1) {
                SDL_RenderDrawPoint(g_renderer, screen_x-1, screen_y);
                SDL_RenderDrawPoint(g_renderer, screen_x+1, screen_y);
                SDL_RenderDrawPoint(g_renderer, screen_x, screen_y-1);
                SDL_RenderDrawPoint(g_renderer, screen_x, screen_y+1);
            }
        } else {
            for (int dx = -scaled_radius; dx <= scaled_radius; dx++) {
                int dy_max = (int)sqrt(scaled_radius * scaled_radius - dx * dx);
                for (int dy = -dy_max; dy <= dy_max; dy++) {
                    int px = screen_x + dx;
                    int py = screen_y + dy;
                    if (px >= 0 && px < WINDOW_WIDTH && py >= 0 && py < WINDOW_HEIGHT) {
                        SDL_RenderDrawPoint(g_renderer, px, py);
                    }
                }
            }
        }
    }
    
    SDL_RenderPresent(g_renderer);
}