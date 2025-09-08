#include <SDL2/SDL.h>
#include <math.h>

#include "types.h"
#include "rendering.h"
#include "simulation.h"
#include "camera.h"
#include "plants.h"
#include "fish.h"
#include "nutrition.h"
#include "gas.h"

static SDL_Renderer* g_renderer = NULL;

static void calculate_aged_color(int base_r, int base_g, int base_b, int age, int age_mature, int* aged_r, int* aged_g, int* aged_b) {
    if (age_mature <= 0) age_mature = 1800; // Fallback value
    
    // Calculate aging factor from 0.0 (young) to 1.0 (fully mature)
    float age_factor = fminf((float)age / (float)age_mature, 1.0f);
    
    // Progressive color darkening with subtle brown tones
    // Young plants: original color
    // Mature plants: darker version with slight brown tint
    
    // Brown target color (subtle brownish tone)
    int brown_r = 101; // RGB(101, 67, 33) - dark brown
    int brown_g = 67;
    int brown_b = 33;
    
    // Reduced brown influence - more darkening than browning
    float brown_influence = age_factor * 0.35f; // Max 35% brown influence (halved)
    float original_influence = 1.0f - brown_influence;
    
    // Stronger darkening effect for aging
    float darkness_factor = 1.0f - (age_factor * 0.15f);
    
    *aged_r = (int)((base_r * original_influence + brown_r * brown_influence) * darkness_factor);
    *aged_g = (int)((base_g * original_influence + brown_g * brown_influence) * darkness_factor);
    *aged_b = (int)((base_b * original_influence + brown_b * brown_influence) * darkness_factor);
    
    // Ensure minimum visibility (not completely black)
    if (*aged_r < 20) *aged_r = 20;
    if (*aged_g < 15) *aged_g = 15;
    if (*aged_b < 10) *aged_b = 10;
    
    // Ensure maximum values don't exceed 255
    if (*aged_r > 255) *aged_r = 255;
    if (*aged_g > 255) *aged_g = 255;
    if (*aged_b > 255) *aged_b = 255;
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
    nutrition_set_renderer(renderer);
    gas_set_renderer(renderer);
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
    
    // Render layers in order (behind everything)
    nutrition_render();
    gas_render();
    
    // Calculate viewport bounds for culling
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    Node* nodes = simulation_get_nodes();
    Chain* chains = simulation_get_chains();
    int node_count = simulation_get_node_count();
    int chain_count = simulation_get_chain_count();
    int selected_node = simulation_get_selected_node();
    int selection_mode = simulation_get_selection_mode();
    
    // Render chains (only for plants, fish don't have chains)
    for (int i = 0; i < chain_count; i++) {
        if (!chains[i].active) continue;
        
        int n1 = chains[i].node1;
        int n2 = chains[i].node2;
        
        if (n1 < 0 || n1 >= node_count || n2 < 0 || n2 >= node_count) continue;
        if (!nodes[n1].active || !nodes[n2].active) continue;
        
        // Skip if either node is a fish node
        if (nodes[n1].plant_type == -1 || nodes[n2].plant_type == -1) continue;
        
        // Frustum culling
        float min_x = fminf(nodes[n1].x, nodes[n2].x);
        float max_x = fmaxf(nodes[n1].x, nodes[n2].x);
        float min_y = fminf(nodes[n1].y, nodes[n2].y);
        float max_y = fmaxf(nodes[n1].y, nodes[n2].y);
        
        if (max_x < world_left || min_x > world_right || 
            max_y < world_top || min_y > world_bottom) {
            continue;
        }
        
        // Get plant type and set chain color
        int plant_type = chains[i].plant_type;
        if (plant_type >= 0 && plant_type < plants_get_type_count()) {
            PlantType* pt = plants_get_type(plant_type);
            if (pt) {
                int aged_r, aged_g, aged_b;
                calculate_aged_color(pt->chain_r, pt->chain_g, pt->chain_b, chains[i].age, pt->age_mature, &aged_r, &aged_g, &aged_b);
                SDL_SetRenderDrawColor(g_renderer, aged_r, aged_g, aged_b, 255);
            } else {
                SDL_SetRenderDrawColor(g_renderer, 100, 200, 100, 255); // Fallback green
            }
        } else {
            SDL_SetRenderDrawColor(g_renderer, 100, 200, 100, 255); // Fallback green
        }
        
        int screen_x1, screen_y1, screen_x2, screen_y2;
        camera_world_to_screen(nodes[n1].x, nodes[n1].y, &screen_x1, &screen_y1);
        camera_world_to_screen(nodes[n2].x, nodes[n2].y, &screen_x2, &screen_y2);
        
        int thickness = (int)(CHAIN_THICKNESS * camera_get_zoom());
        if (thickness < 2) thickness = 2;
        
        draw_curved_line(g_renderer, screen_x1, screen_y1, screen_x2, screen_y2, 
                        chains[i].curve_strength, chains[i].curve_offset, thickness);
    }
    
    // Get fish data for rendering
    Fish* fish_list = fish_get_all();
    int fish_count = fish_get_count();
    
    // Render nodes (both plants and fish)
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
        
        // Check if this is a fish node
        if (nodes[i].plant_type == -1) {
            // This is a fish node - find the fish that owns this node
            Fish* fish = NULL;
            for (int f = 0; f < fish_count; f++) {
                if (fish_list[f].active && fish_list[f].node_id == i) {
                    fish = &fish_list[f];
                    break;
                }
            }
            
            if (fish) {
                FishType* fish_type = fish_get_type(fish->fish_type);
                if (fish_type) {
                    // Use fish type radius and color - make fish larger than plants
                    scaled_radius = (int)(fish_type->size_radius * camera_get_zoom() * 1.5f); // 1.5x larger than config
                    if (scaled_radius < 4) scaled_radius = 4; // Minimum size for visibility
                    
                    SDL_SetRenderDrawColor(g_renderer, fish_type->node_r, fish_type->node_g, fish_type->node_b, 255);
                } else {
                    scaled_radius = (int)(12 * camera_get_zoom()); // Fallback size
                    if (scaled_radius < 4) scaled_radius = 4;
                    SDL_SetRenderDrawColor(g_renderer, 255, 165, 0, 255); // Orange fallback for fish
                }
            } else {
                scaled_radius = (int)(12 * camera_get_zoom()); // Fallback size
                if (scaled_radius < 4) scaled_radius = 4;
                SDL_SetRenderDrawColor(g_renderer, 255, 0, 255, 255); // Magenta for orphaned fish nodes
            }
        } else {
            // Plant node
            if (scaled_radius < 1) scaled_radius = 1;
            
            // Set color based on selection state and plant type
            if (i == selected_node && selection_mode == 1) {
                SDL_SetRenderDrawColor(g_renderer, 255, 255, 0, 255); // Yellow for selected
            } else {
                int plant_type = nodes[i].plant_type;
                if (plant_type >= 0 && plant_type < plants_get_type_count()) {
                    PlantType* pt = plants_get_type(plant_type);
                    if (pt) {
                        int aged_r, aged_g, aged_b;
                        calculate_aged_color(pt->node_r, pt->node_g, pt->node_b, nodes[i].age, pt->age_mature, &aged_r, &aged_g, &aged_b);
                        SDL_SetRenderDrawColor(g_renderer, aged_r, aged_g, aged_b, 255);
                    } else {
                        SDL_SetRenderDrawColor(g_renderer, 150, 255, 150, 255); // Fallback light green
                    }
                } else {
                    SDL_SetRenderDrawColor(g_renderer, 150, 255, 150, 255); // Fallback light green
                }
            }
        }
        
        // Draw node as circle (both plants and fish use same circle rendering)
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