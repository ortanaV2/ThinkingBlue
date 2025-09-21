// rendering.c - Enhanced with flow-based water background coloring
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
#include "flow.h"
#include "temperature.h"

static SDL_Renderer* g_renderer = NULL;

static void draw_thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness);
static void draw_curved_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, float curve_strength, float curve_offset, int thickness);
static void draw_fish_tail(SDL_Renderer* renderer, int screen_x, int screen_y, float heading, int fish_radius, int r, int g, int b);
static void draw_fish_rl_vision(SDL_Renderer* renderer, int fish_id);

// Enhanced flow-based water background rendering with higher resolution and smoothing
static void render_flow_based_water_background(void) {
    // Get viewport bounds
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    // Base water color (original blue)
    int base_r = 22;
    int base_g = 117; 
    int base_b = 158;
    
    // Very early scaling resolution: starts improving quality at zoom 0.3+
    float zoom = camera_get_zoom();
    int grid_size;
    
    if (zoom >= 0.3f) {
        // Start high quality very early: from zoom 0.3 onwards
        float quality_factor = 0.8f + (zoom - 0.3f) * 6.0f;  // Very aggressive scaling from 0.3+
        grid_size = (int)(LAYER_GRID_SIZE / quality_factor);
        if (grid_size < 2) grid_size = 2;    // Ultra high detail when zoomed in
        if (grid_size > 25) grid_size = 25;  // Reasonable maximum
    } else {
        // Very zoomed out: reduce quality significantly for performance
        grid_size = (int)(LAYER_GRID_SIZE * (3.0f - zoom * 5.0f));  // Very aggressive reduction
        if (grid_size < 25) grid_size = 25;   // Minimum when zoomed out
        if (grid_size > 40) grid_size = 40; // Maximum for very far zoom
    }
    
    // Calculate grid dimensions for smoothing buffer
    int grid_width = (int)ceil((world_right - world_left) / grid_size) + 2;
    int grid_height = (int)ceil((world_bottom - world_top) / grid_size) + 2;
    
    // Allocate temporary buffers for flow values and smoothed colors
    float* flow_magnitudes = malloc(grid_width * grid_height * sizeof(float));
    int* smoothed_r = malloc(grid_width * grid_height * sizeof(int));
    int* smoothed_g = malloc(grid_width * grid_height * sizeof(int));
    int* smoothed_b = malloc(grid_width * grid_height * sizeof(int));
    
    if (!flow_magnitudes || !smoothed_r || !smoothed_g || !smoothed_b) {
        // Fallback to original method if allocation fails
        free(flow_magnitudes);
        free(smoothed_r);
        free(smoothed_g);
        free(smoothed_b);
        
        // Fallback rendering with very early scaling quality
        int fallback_grid_size;
        if (zoom >= 0.3f) {
            float quality_factor = 0.8f + (zoom - 0.3f) * 3.0f;
            fallback_grid_size = (int)(LAYER_GRID_SIZE / quality_factor);
            if (fallback_grid_size < 6) fallback_grid_size = 6;
            if (fallback_grid_size > 50) fallback_grid_size = 50;
        } else {
            fallback_grid_size = (int)(LAYER_GRID_SIZE * (4.0f - zoom * 8.0f));
            if (fallback_grid_size < 50) fallback_grid_size = 50;
            if (fallback_grid_size > 200) fallback_grid_size = 200;
        }
        
        for (float world_y = world_top; world_y < world_bottom; world_y += fallback_grid_size) {
            for (float world_x = world_left; world_x < world_right; world_x += fallback_grid_size) {
                float flow_x, flow_y;
                flow_get_vector_at(world_x, world_y, &flow_x, &flow_y);
                float flow_magnitude = sqrt(flow_x * flow_x + flow_y * flow_y);
                float normalized_flow = flow_magnitude / 0.4f;
                if (normalized_flow > 1.0f) normalized_flow = 1.0f;
                
                int water_r = base_r, water_g = base_g, water_b = base_b;
                
                int screen_x1, screen_y1, screen_x2, screen_y2;
                camera_world_to_screen(world_x, world_y, &screen_x1, &screen_y1);
                camera_world_to_screen(world_x + fallback_grid_size, world_y + fallback_grid_size, &screen_x2, &screen_y2);
                
                if (screen_x2 > 0 && screen_x1 < WINDOW_WIDTH && screen_y2 > 0 && screen_y1 < WINDOW_HEIGHT) {
                    SDL_SetRenderDrawColor(g_renderer, water_r, water_g, water_b, 255);
                    SDL_Rect water_rect = {screen_x1, screen_y1, screen_x2 - screen_x1, screen_y2 - screen_y1};
                    SDL_RenderFillRect(g_renderer, &water_rect);
                }
            }
        }
        return;
    }
    
    // Sample flow field at higher resolution and store values
    int grid_idx = 0;
    for (int gy = 0; gy < grid_height; gy++) {
        for (int gx = 0; gx < grid_width; gx++) {
            float world_x = world_left + (gx - 1) * grid_size;
            float world_y = world_top + (gy - 1) * grid_size;
            
            float flow_x, flow_y;
            flow_get_vector_at(world_x, world_y, &flow_x, &flow_y);
            float flow_magnitude = sqrt(flow_x * flow_x + flow_y * flow_y);
            
            flow_magnitudes[grid_idx] = flow_magnitude / 0.8f;  // Normalize
            if (flow_magnitudes[grid_idx] > 1.0f) flow_magnitudes[grid_idx] = 1.0f;
            
            grid_idx++;
        }
    }
    
    // Apply adaptive smoothing based on zoom level (starts very early)
    float smoothness;
    if (zoom >= 0.3f) {
        smoothness = 0.6f + (zoom - 0.3f) * 1.5f;  // Smoothing starts at zoom 0.3
    } else {
        smoothness = 0.2f + zoom * 1.3f;  // Minimal smoothing when very zoomed out
    }
    grid_idx = 0;
    
    for (int gy = 0; gy < grid_height; gy++) {
        for (int gx = 0; gx < grid_width; gx++) {
            float sum_flow = 0.0f;
            float weight_sum = 0.0f;
            
            // 3x3 smoothing kernel
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = gx + dx;
                    int ny = gy + dy;
                    
                    if (nx >= 0 && nx < grid_width && ny >= 0 && ny < grid_height) {
                        float distance = sqrt(dx * dx + dy * dy);
                        float weight = exp(-distance * distance / (2.0f * smoothness));
                        
                        sum_flow += flow_magnitudes[ny * grid_width + nx] * weight;
                        weight_sum += weight;
                    }
                }
            }
            
            float smoothed_flow = (weight_sum > 0.0f) ? (sum_flow / weight_sum) : flow_magnitudes[grid_idx];
            
            // Calculate smoothed color based on flow
            int water_r, water_g, water_b;
            
            if (smoothed_flow < 0.1f) {
                // Very weak flow: slightly lighter blue (clear water)
                water_r = base_r + (int)(8 * (0.1f - smoothed_flow) / 0.1f);
                water_g = base_g + (int)(12 * (0.1f - smoothed_flow) / 0.1f);
                water_b = base_b + (int)(15 * (0.1f - smoothed_flow) / 0.1f);
            } else if (smoothed_flow < 0.3f) {
                // Weak flow: normal water color
                water_r = base_r;
                water_g = base_g;
                water_b = base_b;
            } else if (smoothed_flow < 0.6f) {
                // Medium flow: slightly darker, more greenish
                float factor = (smoothed_flow - 0.3f) / 0.3f;
                water_r = base_r - (int)(6 * factor);
                water_g = base_g - (int)(5 * factor);
                water_b = base_b - (int)(8 * factor);
            } else {
                // Strong flow: darker, more greenish-brown
                float factor = (smoothed_flow - 0.6f) / 0.4f;
                water_r = base_r - 6 - (int)(8 * factor);
                water_g = base_g - 5 - (int)(3 * factor);
                water_b = base_b - 8 - (int)(12 * factor);
            }
            
            // Clamp colors
            if (water_r < 10) water_r = 10;
            if (water_g < 90) water_g = 90;
            if (water_b < 120) water_b = 120;
            if (water_r > 35) water_r = 35;
            if (water_g > 130) water_g = 130;
            if (water_b > 180) water_b = 180;
            
            smoothed_r[grid_idx] = water_r;
            smoothed_g[grid_idx] = water_g;
            smoothed_b[grid_idx] = water_b;
            
            grid_idx++;
        }
    }
    
    // Render smoothed water tiles
    grid_idx = 0;
    for (int gy = 1; gy < grid_height - 1; gy++) {  // Skip border cells
        for (int gx = 1; gx < grid_width - 1; gx++) {
            float world_x = world_left + (gx - 1) * grid_size;
            float world_y = world_top + (gy - 1) * grid_size;
            
            int idx = gy * grid_width + gx;
            
            // Convert world coordinates to screen coordinates
            int screen_x1, screen_y1, screen_x2, screen_y2;
            camera_world_to_screen(world_x, world_y, &screen_x1, &screen_y1);
            camera_world_to_screen(world_x + grid_size, world_y + grid_size, &screen_x2, &screen_y2);
            
            // Only render if on screen
            if (screen_x2 > 0 && screen_x1 < WINDOW_WIDTH && 
                screen_y2 > 0 && screen_y1 < WINDOW_HEIGHT) {
                
                SDL_SetRenderDrawColor(g_renderer, smoothed_r[idx], smoothed_g[idx], smoothed_b[idx], 255);
                
                SDL_Rect water_rect;
                water_rect.x = screen_x1;
                water_rect.y = screen_y1;
                water_rect.w = screen_x2 - screen_x1;
                water_rect.h = screen_y2 - screen_y1;
                
                SDL_RenderFillRect(g_renderer, &water_rect);
            }
        }
    }
    
    // Clean up temporary buffers
    free(flow_magnitudes);
    free(smoothed_r);
    free(smoothed_g);
    free(smoothed_b);
}

static void calculate_aged_color(int base_r, int base_g, int base_b, int age, int age_mature, int* aged_r, int* aged_g, int* aged_b) {
    if (age_mature <= 0) age_mature = 1800;
    
    float age_factor = fminf((float)age / (float)age_mature, 1.0f);
    
    int brown_r = 101;
    int brown_g = 67;
    int brown_b = 33;
    
    float brown_influence = age_factor * 0.20f;
    float original_influence = 1.0f - brown_influence;
    float darkness_factor = 1.0f - (age_factor * 0.08f);
    
    *aged_r = (int)((base_r * original_influence + brown_r * brown_influence) * darkness_factor);
    *aged_g = (int)((base_g * original_influence + brown_g * brown_influence) * darkness_factor);
    *aged_b = (int)((base_b * original_influence + brown_b * brown_influence) * darkness_factor);
    
    if (*aged_r < 20) *aged_r = 20;
    if (*aged_g < 15) *aged_g = 15;
    if (*aged_b < 10) *aged_b = 10;
    
    if (*aged_r > 255) *aged_r = 255;
    if (*aged_g > 255) *aged_g = 255;
    if (*aged_b > 255) *aged_b = 255;
}

static void calculate_bleached_color(int base_r, int base_g, int base_b, int* bleached_r, int* bleached_g, int* bleached_b) {
    // Convert to grayscale and make it very light (bleached appearance)
    int gray_value = (int)(0.299f * base_r + 0.587f * base_g + 0.114f * base_b);
    
    // Make it very light gray/white for bleached effect
    *bleached_r = (gray_value + 255) / 2;
    *bleached_g = (gray_value + 255) / 2;
    *bleached_b = (gray_value + 255) / 2;
    
    // Ensure minimum whiteness
    if (*bleached_r < 200) *bleached_r = 200;
    if (*bleached_g < 200) *bleached_g = 200;
    if (*bleached_b < 200) *bleached_b = 200;
}

static void calculate_corpse_color(int original_fish_type, int decay_timer, int* corpse_r, int* corpse_g, int* corpse_b) {
    // Base corpse color is white/gray
    int base_gray = 220;
    
    // Darken based on decay progress
    float decay_factor = (float)decay_timer / (float)CORPSE_DECAY_TIME;
    
    // Fresh corpse is lighter, old corpse is darker
    int gray_value = (int)(base_gray * (0.5f + decay_factor * 0.5f));
    
    // Slight color tint based on original fish type
    FishType* original_type = fish_get_type(original_fish_type);
    if (original_type) {
        // Very subtle tint from original colors
        *corpse_r = (gray_value * 9 + original_type->node_r) / 10;
        *corpse_g = (gray_value * 9 + original_type->node_g) / 10;
        *corpse_b = (gray_value * 9 + original_type->node_b) / 10;
    } else {
        *corpse_r = gray_value;
        *corpse_g = gray_value;
        *corpse_b = gray_value;
    }
    
    // Ensure minimum brightness for visibility
    if (*corpse_r < 150) *corpse_r = 150;
    if (*corpse_g < 150) *corpse_g = 150;
    if (*corpse_b < 150) *corpse_b = 150;
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

static void draw_fish_tail(SDL_Renderer* renderer, int screen_x, int screen_y, float heading, int fish_radius, int r, int g, int b) {
    // Draw tail based on fish heading
    float tail_offset_distance = fish_radius * 3.0f;
    
    // Calculate tail base position (opposite to heading direction)
    int tail_base_x = screen_x - (int)(cos(heading) * tail_offset_distance);
    int tail_base_y = screen_y - (int)(sin(heading) * tail_offset_distance);
    
    float tail_length = fish_radius * 2.5f;
    float tail_width = fish_radius * 3.0f;
    
    // Perpendicular vector for triangle width
    float perp_x = -sin(heading);
    float perp_y = cos(heading);
    
    // Two tail corners
    int tail_corner1_x = tail_base_x + (int)(perp_x * tail_width * 0.5f);
    int tail_corner1_y = tail_base_y + (int)(perp_y * tail_width * 0.5f);
    int tail_corner2_x = tail_base_x - (int)(perp_x * tail_width * 0.5f);
    int tail_corner2_y = tail_base_y - (int)(perp_y * tail_width * 0.5f);
    
    // Third corner (tip of tail)
    int tail_tip_x = tail_base_x + (int)(cos(heading) * tail_length);
    int tail_tip_y = tail_base_y + (int)(sin(heading) * tail_length);
    
    // Make tail color slightly darker
    int tail_r = (r * 3) / 4;
    int tail_g = (g * 3) / 4;
    int tail_b = (b * 3) / 4;
    SDL_SetRenderDrawColor(renderer, tail_r, tail_g, tail_b, 255);
    
    // Draw triangle outline
    SDL_RenderDrawLine(renderer, tail_base_x, tail_base_y, tail_corner1_x, tail_corner1_y);
    SDL_RenderDrawLine(renderer, tail_corner1_x, tail_corner1_y, tail_tip_x, tail_tip_y);
    SDL_RenderDrawLine(renderer, tail_tip_x, tail_tip_y, tail_corner2_x, tail_corner2_y);
    SDL_RenderDrawLine(renderer, tail_corner2_x, tail_corner2_y, tail_base_x, tail_base_y);
    
    // Simple fill
    if (tail_width > 2.0f) {
        for (int i = 0; i <= (int)(tail_length); i++) {
            float t = (float)i / tail_length;
            
            int edge_x1 = tail_corner1_x + (int)(t * (tail_tip_x - tail_corner1_x));
            int edge_y1 = tail_corner1_y + (int)(t * (tail_tip_y - tail_corner1_y));
            int edge_x2 = tail_corner2_x + (int)(t * (tail_tip_x - tail_corner2_x));
            int edge_y2 = tail_corner2_y + (int)(t * (tail_tip_y - tail_corner2_y));
            
            SDL_RenderDrawLine(renderer, tail_base_x, tail_base_y, edge_x1, edge_y1);
            SDL_RenderDrawLine(renderer, tail_base_x, tail_base_y, edge_x2, edge_y2);
            SDL_RenderDrawLine(renderer, edge_x1, edge_y1, edge_x2, edge_y2);
        }
    }
}

static void draw_fish_rl_vision(SDL_Renderer* renderer, int fish_id) {
    if (!fish_is_ray_rendering_enabled()) return;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
    // Get fish position on screen
    int fish_screen_x, fish_screen_y;
    camera_world_to_screen(fish_node->x, fish_node->y, &fish_screen_x, &fish_screen_y);
    
    // Draw FOV arc
    float fov_rad = (fish_type->fov_angle * M_PI) / 180.0f;
    float half_fov = fov_rad * 0.5f;
    float fov_range = 200.0f;
    
    SDL_SetRenderDrawColor(renderer, 100, 150, 255, 100);
    
    // Draw FOV boundary lines
    float left_angle = fish->heading - half_fov;
    float right_angle = fish->heading + half_fov;
    
    int left_end_x = fish_screen_x + (int)(cos(left_angle) * fov_range * camera_get_zoom());
    int left_end_y = fish_screen_y + (int)(sin(left_angle) * fov_range * camera_get_zoom());
    int right_end_x = fish_screen_x + (int)(cos(right_angle) * fov_range * camera_get_zoom());
    int right_end_y = fish_screen_y + (int)(sin(right_angle) * fov_range * camera_get_zoom());
    
    SDL_RenderDrawLine(renderer, fish_screen_x, fish_screen_y, left_end_x, left_end_y);
    SDL_RenderDrawLine(renderer, fish_screen_x, fish_screen_y, right_end_x, right_end_y);
    
    // Draw plant detection vector if present
    float plant_vec_x = fish->rl_inputs[0];
    float plant_vec_y = fish->rl_inputs[1];
    
    if (plant_vec_x != 0.0f || plant_vec_y != 0.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 200);
        
        float plant_range = 150.0f;
        int plant_end_x = fish_screen_x + (int)(plant_vec_x * plant_range * camera_get_zoom());
        int plant_end_y = fish_screen_y + (int)(plant_vec_y * plant_range * camera_get_zoom());
        
        // Draw thick arrow to plant
        for (int i = -2; i <= 2; i++) {
            SDL_RenderDrawLine(renderer, fish_screen_x + i, fish_screen_y, plant_end_x + i, plant_end_y);
            SDL_RenderDrawLine(renderer, fish_screen_x, fish_screen_y + i, plant_end_x, plant_end_y + i);
        }
        
        // Draw arrowhead
        float arrow_angle = atan2(plant_vec_y, plant_vec_x);
        float arrow_size = 10.0f * camera_get_zoom();
        
        int arrow1_x = plant_end_x - (int)(cos(arrow_angle - 0.5f) * arrow_size);
        int arrow1_y = plant_end_y - (int)(sin(arrow_angle - 0.5f) * arrow_size);
        int arrow2_x = plant_end_x - (int)(cos(arrow_angle + 0.5f) * arrow_size);
        int arrow2_y = plant_end_y - (int)(sin(arrow_angle + 0.5f) * arrow_size);
        
        SDL_RenderDrawLine(renderer, plant_end_x, plant_end_y, arrow1_x, arrow1_y);
        SDL_RenderDrawLine(renderer, plant_end_x, plant_end_y, arrow2_x, arrow2_y);
    }
    
    // Draw heading direction
    SDL_SetRenderDrawColor(renderer, 255, 255, 100, 150);
    
    int heading_end_x = fish_screen_x + (int)(cos(fish->heading) * 50.0f * camera_get_zoom());
    int heading_end_y = fish_screen_y + (int)(sin(fish->heading) * 50.0f * camera_get_zoom());
    
    draw_thick_line(renderer, fish_screen_x, fish_screen_y, heading_end_x, heading_end_y, 3);
}

// Enhanced curved line drawing with configurable curvature
static void draw_curved_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, float curve_strength, float curve_offset, int thickness) {
    float mid_x = (x1 + x2) * 0.5f;
    float mid_y = (y1 + y2) * 0.5f;
    
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx * dx + dy * dy);
    
    if (length < 1.0f) {
        draw_thick_line(renderer, x1, y1, x2, y2, thickness);
        return;
    }
    
    dx /= length;
    dy /= length;
    
    float perp_x = -dy;
    float perp_y = dx;
    
    float curve_amount = curve_strength * length * 0.3f + curve_offset;
    float ctrl_x = mid_x + perp_x * curve_amount;
    float ctrl_y = mid_y + perp_y * curve_amount;
    
    int segments = (int)(length / 8.0f) + 3;
    if (segments > 20) segments = 20;
    
    float prev_x = x1;
    float prev_y = y1;
    
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / segments;
        float inv_t = 1.0f - t;
        
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
    flow_set_renderer(renderer);
    return 1;
}

void rendering_cleanup(void) {
    g_renderer = NULL;
}

void rendering_render(void) {
    if (!g_renderer) return;
    
    // ENHANCED: Render flow-based water background instead of solid color
    render_flow_based_water_background();
    
    // Render layers
    nutrition_render();
    gas_render();
    flow_render();
    
    // Calculate viewport bounds for culling
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    Node* nodes = simulation_get_nodes();
    Chain* chains = simulation_get_chains();
    int node_count = simulation_get_node_count();
    int chain_count = simulation_get_chain_count();
    int selected_node = simulation_get_selected_node();
    int selection_mode = simulation_get_selection_mode();
    
    // Enhanced render chains with configurable thickness and curvature
    for (int i = 0; i < chain_count; i++) {
        if (!chains[i].active) continue;
        
        int n1 = chains[i].node1;
        int n2 = chains[i].node2;
        
        if (n1 < 0 || n1 >= node_count || n2 < 0 || n2 >= node_count) continue;
        if (!nodes[n1].active || !nodes[n2].active) continue;
        
        // Skip chains involving fish nodes or corpses
        if (nodes[n1].plant_type == -1 || nodes[n2].plant_type == -1) continue;
        if (nodes[n1].is_corpse || nodes[n2].is_corpse) continue;
        
        // Frustum culling
        float min_x = fminf(nodes[n1].x, nodes[n2].x);
        float max_x = fmaxf(nodes[n1].x, nodes[n2].x);
        float min_y = fminf(nodes[n1].y, nodes[n2].y);
        float max_y = fmaxf(nodes[n1].y, nodes[n2].y);
        
        if (max_x < world_left || min_x > world_right || 
            max_y < world_top || min_y > world_bottom) {
            continue;
        }
        
        // Get plant type for visual configuration
        int plant_type = chains[i].plant_type;
        PlantType* pt = plants_get_type(plant_type);
        
        // Set chain color - check for bleaching
        if (pt && pt->active) {
            int aged_r, aged_g, aged_b;
            calculate_aged_color(pt->chain_r, pt->chain_g, pt->chain_b, chains[i].age, pt->age_mature, &aged_r, &aged_g, &aged_b);
            
            // Check if either node is bleached
            if (temperature_is_coral_bleached(n1) || temperature_is_coral_bleached(n2)) {
                int bleached_r, bleached_g, bleached_b;
                calculate_bleached_color(aged_r, aged_g, aged_b, &bleached_r, &bleached_g, &bleached_b);
                SDL_SetRenderDrawColor(g_renderer, bleached_r, bleached_g, bleached_b, 255);
            } else {
                SDL_SetRenderDrawColor(g_renderer, aged_r, aged_g, aged_b, 255);
            }
        } else {
            SDL_SetRenderDrawColor(g_renderer, 100, 200, 100, 255);
        }
        
        int screen_x1, screen_y1, screen_x2, screen_y2;
        camera_world_to_screen(nodes[n1].x, nodes[n1].y, &screen_x1, &screen_y1);
        camera_world_to_screen(nodes[n2].x, nodes[n2].y, &screen_x2, &screen_y2);
        
        // Enhanced calculate thickness with plant-specific factor
        float thickness_factor = pt ? pt->chain_thickness_factor : 1.0f;
        int thickness = (int)(CHAIN_THICKNESS * camera_get_zoom() * thickness_factor);
        if (thickness < 2) thickness = 2;
        
        // Enhanced apply curvature multiplier
        float final_curve_strength = chains[i].curve_strength * chains[i].curve_multiplier;
        
        draw_curved_line(g_renderer, screen_x1, screen_y1, screen_x2, screen_y2, 
                        final_curve_strength, chains[i].curve_offset, thickness);
    }
    
    // Get fish data for rendering
    Fish* fish_list = fish_get_all();
    int fish_count = fish_get_count();
    
    // Render fish RL vision rays FIRST (behind fish)
    if (fish_is_ray_rendering_enabled()) {
        for (int i = 0; i < fish_count; i++) {
            if (fish_list[i].active) {
                draw_fish_rl_vision(g_renderer, i);
            }
        }
    }
    
    // Enhanced render nodes with configurable sizes and seed immunity
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
        
        // Check if this is a corpse node
        if (nodes[i].is_corpse) {
            // Render corpse with white/gray color and tail
            if (scaled_radius < 1) scaled_radius = 1;
            
            int corpse_r, corpse_g, corpse_b;
            calculate_corpse_color(nodes[i].original_fish_type, nodes[i].corpse_decay_timer, &corpse_r, &corpse_g, &corpse_b);
            SDL_SetRenderDrawColor(g_renderer, corpse_r, corpse_g, corpse_b, 255);
            
            // Make corpse slightly larger than regular nodes
            scaled_radius = (int)((NODE_RADIUS * 1.5f) * camera_get_zoom());
            if (scaled_radius < 1) scaled_radius = 1;
            
            // Draw corpse tail FIRST
            if (scaled_radius > 2) {
                draw_fish_tail(g_renderer, screen_x, screen_y, nodes[i].corpse_heading, scaled_radius, corpse_r, corpse_g, corpse_b);
            }
            
            // Draw corpse body
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
            
            // Draw decay indicator
            float decay_progress = 1.0f - ((float)nodes[i].corpse_decay_timer / (float)CORPSE_DECAY_TIME);
            if (decay_progress > 0.5f) {
                SDL_SetRenderDrawColor(g_renderer, 100, 100, 100, 255);
                for (int angle = 0; angle < 360; angle += 45) {
                    float rad = angle * M_PI / 180.0f;
                    int outline_x = screen_x + (int)(cos(rad) * scaled_radius);
                    int outline_y = screen_y + (int)(sin(rad) * scaled_radius);
                    SDL_RenderDrawPoint(g_renderer, outline_x, outline_y);
                }
            }
            
            continue;
        }
        
        // Check if this is a fish node
        if (nodes[i].plant_type == -1) {
            // Fish node rendering
            Fish* fish = NULL;
            for (int f = 0; f < fish_count; f++) {
                if (fish_list[f].active && fish_list[f].node_id == i) {
                    fish = &fish_list[f];
                    break;
                }
            }
            
            int fish_r = 255, fish_g = 165, fish_b = 0;
            
            if (fish) {
                FishType* fish_type = fish_get_type(fish->fish_type);
                if (fish_type && fish_type->active) {
                    scaled_radius = (int)((NODE_RADIUS * 1.8f) * camera_get_zoom());
                    if (scaled_radius < 1) scaled_radius = 1;
                    
                    fish_r = fish_type->node_r;
                    fish_g = fish_type->node_g;
                    fish_b = fish_type->node_b;
                    SDL_SetRenderDrawColor(g_renderer, fish_r, fish_g, fish_b, 255);
                } else {
                    scaled_radius = (int)((NODE_RADIUS * 1.8f) * camera_get_zoom());
                    if (scaled_radius < 1) scaled_radius = 1;
                    SDL_SetRenderDrawColor(g_renderer, fish_r, fish_g, fish_b, 255);
                }
            } else {
                scaled_radius = (int)((NODE_RADIUS * 1.8f) * camera_get_zoom());
                if (scaled_radius < 1) scaled_radius = 1;
                fish_r = 255; fish_g = 0; fish_b = 255;
                SDL_SetRenderDrawColor(g_renderer, fish_r, fish_g, fish_b, 255);
            }
            
            // Draw fish tail FIRST
            if (scaled_radius > 2 && fish) {
                draw_fish_tail(g_renderer, screen_x, screen_y, fish->heading, scaled_radius, fish_r, fish_g, fish_b);
            }
            
            // Draw fish body
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
            
            continue;
        } else {
            // Enhanced plant node with configurable size and seed immunity visual
            int plant_type = nodes[i].plant_type;
            PlantType* pt = plants_get_type(plant_type);
            
            // Apply size factor from plant configuration
            float size_factor = pt ? pt->node_size_factor : 1.0f;
            scaled_radius = (int)(NODE_RADIUS * camera_get_zoom() * size_factor);
            if (scaled_radius < 1) scaled_radius = 1;
            
            // Set color based on selection state, plant type, bleaching, and seed immunity
            if (i == selected_node && selection_mode == 1) {
                SDL_SetRenderDrawColor(g_renderer, 255, 255, 0, 255);
            } else {
                if (pt && pt->active) {
                    int aged_r, aged_g, aged_b;
                    calculate_aged_color(pt->node_r, pt->node_g, pt->node_b, nodes[i].age, pt->age_mature, &aged_r, &aged_g, &aged_b);
                    
                    // Check if coral is bleached
                    if (temperature_is_coral_bleached(i)) {
                        int bleached_r, bleached_g, bleached_b;
                        calculate_bleached_color(aged_r, aged_g, aged_b, &bleached_r, &bleached_g, &bleached_b);
                        SDL_SetRenderDrawColor(g_renderer, bleached_r, bleached_g, bleached_b, 255);
                    } 
                    // Check if seed has immunity
                    else if (nodes[i].seed_immunity_timer > 0) {
                        // Immune seeds get a bright, pulsing color
                        float immunity_ratio = (float)nodes[i].seed_immunity_timer / (float)SEED_IMMUNITY_TIME;
                        
                        // Pulsing effect based on frame counter
                        int pulse_frame = simulation_get_frame_counter() % 60;  // 2 second cycle at 30 FPS
                        float pulse_factor = 0.7f + 0.3f * sin((pulse_frame / 60.0f) * 2.0f * M_PI);
                        
                        // Bright green-white color for immune seeds
                        int immune_r = (int)(255 * pulse_factor);
                        int immune_g = (int)(255 * pulse_factor);  
                        int immune_b = (int)(200 * pulse_factor);
                        
                        // Mix with original color based on immunity remaining
                        int final_r = (int)(aged_r * (1.0f - immunity_ratio) + immune_r * immunity_ratio);
                        int final_g = (int)(aged_g * (1.0f - immunity_ratio) + immune_g * immunity_ratio);
                        int final_b = (int)(aged_b * (1.0f - immunity_ratio) + immune_b * immunity_ratio);
                        
                        SDL_SetRenderDrawColor(g_renderer, final_r, final_g, final_b, 255);
                    } 
                    else {
                        SDL_SetRenderDrawColor(g_renderer, aged_r, aged_g, aged_b, 255);
                    }
                } else {
                    SDL_SetRenderDrawColor(g_renderer, 150, 255, 150, 255);
                }
            }
        }
        
        // Draw plant node as circle
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