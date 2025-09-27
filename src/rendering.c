// Enhanced rendering system with configurable fish visuals and environmental layers
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

// FPS display variables
static float g_current_fps = 0.0f;
static char g_fps_text[32] = "FPS: 0.0";

// Forward declarations
static void draw_thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness);
static void draw_curved_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, float curve_strength, float curve_offset, int thickness);
static void draw_enhanced_fish_tail(SDL_Renderer* renderer, int screen_x, int screen_y, float heading, FishType* fish_type, float base_radius, int r, int g, int b);
static void draw_fish_rl_vision(SDL_Renderer* renderer, int fish_id);
static void render_flow_based_water_background(void);

// Find fish by node ID for rendering
static Fish* find_fish_by_node_id(int node_id) {
    Fish* all_fish = fish_get_all();
    int highest_slot = fish_get_highest_slot();
    
    for (int i = 0; i <= highest_slot && i < MAX_FISH; i++) {
        if (all_fish[i].active && all_fish[i].node_id == node_id) {
            return &all_fish[i];
        }
    }
    return NULL;
}

// Simple bitmap font for FPS display
static void draw_simple_char(SDL_Renderer* renderer, char c, int x, int y, int size);
static void draw_simple_text(SDL_Renderer* renderer, const char* text, int x, int y, int size);

static void draw_simple_char(SDL_Renderer* renderer, char c, int x, int y, int size) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    // Simple 5x7 bitmap patterns for digits and letters
    static int patterns[256][7] = {0};
    static int initialized = 0;
    
    if (!initialized) {
        // Numbers 0-9
        int nums[10][7] = {
            {0x1E, 0x21, 0x21, 0x21, 0x21, 0x21, 0x1E}, // 0
            {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
            {0x1E, 0x21, 0x01, 0x0E, 0x10, 0x20, 0x3F}, // 2
            {0x1E, 0x21, 0x01, 0x0E, 0x01, 0x21, 0x1E}, // 3
            {0x02, 0x06, 0x0A, 0x12, 0x3F, 0x02, 0x02}, // 4
            {0x3F, 0x20, 0x3E, 0x01, 0x01, 0x21, 0x1E}, // 5
            {0x0E, 0x10, 0x20, 0x3E, 0x21, 0x21, 0x1E}, // 6
            {0x3F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}, // 7
            {0x1E, 0x21, 0x21, 0x1E, 0x21, 0x21, 0x1E}, // 8
            {0x1E, 0x21, 0x21, 0x1F, 0x01, 0x02, 0x1C}  // 9
        };
        
        // Letters F, P, S and symbols
        int let_F[7] = {0x3E, 0x20, 0x20, 0x3C, 0x20, 0x20, 0x20};
        int let_P[7] = {0x3E, 0x21, 0x21, 0x3E, 0x20, 0x20, 0x20};
        int let_S[7] = {0x1F, 0x20, 0x20, 0x1E, 0x01, 0x01, 0x3E};
        int colon[7] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00};
        int dot[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18};
        
        // Copy patterns to lookup table
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 7; j++) {
                patterns['0' + i][j] = nums[i][j];
            }
        }
        for (int j = 0; j < 7; j++) {
            patterns['F'][j] = let_F[j];
            patterns['P'][j] = let_P[j];
            patterns['S'][j] = let_S[j];
            patterns[':'][j] = colon[j];
            patterns['.'][j] = dot[j];
        }
        initialized = 1;
    }
    
    // Draw character pixel by pixel
    unsigned char uc = (unsigned char)c;
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 6; col++) {
            if (patterns[uc][row] & (1 << (5-col))) {
                SDL_Rect pixel = {x + col * size, y + row * size, size, size};
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
    }
}

static void draw_simple_text(SDL_Renderer* renderer, const char* text, int x, int y, int size) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        draw_simple_char(renderer, text[i], x + i * (6 * size), y, size);
    }
}

// Enhanced flow-based water background with adaptive quality
static void render_flow_based_water_background(void) {
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    // Base ocean water color
    int base_r = 22;
    int base_g = 117; 
    int base_b = 158;
    
    // Adaptive grid size based on zoom level for performance
    float zoom = camera_get_zoom();
    int grid_size;
    
    if (zoom >= 0.3f) {
        // High quality when zoomed in
        float quality_factor = 0.8f + (zoom - 0.3f) * 6.0f;
        grid_size = (int)(LAYER_GRID_SIZE / quality_factor);
        if (grid_size < 2) grid_size = 2;
        if (grid_size > 25) grid_size = 25;
    } else {
        // Lower quality when zoomed out for performance
        grid_size = (int)(LAYER_GRID_SIZE * (3.0f - zoom * 5.0f));
        if (grid_size < 25) grid_size = 25;
        if (grid_size > 40) grid_size = 40;
    }
    
    // Calculate grid dimensions for smoothing
    int grid_width = (int)ceil((world_right - world_left) / grid_size) + 2;
    int grid_height = (int)ceil((world_bottom - world_top) / grid_size) + 2;
    
    // Allocate temporary buffers for flow-based water colors
    float* flow_magnitudes = malloc(grid_width * grid_height * sizeof(float));
    int* smoothed_r = malloc(grid_width * grid_height * sizeof(int));
    int* smoothed_g = malloc(grid_width * grid_height * sizeof(int));
    int* smoothed_b = malloc(grid_width * grid_height * sizeof(int));
    
    if (!flow_magnitudes || !smoothed_r || !smoothed_g || !smoothed_b) {
        // Fallback to simple background if allocation fails
        SDL_SetRenderDrawColor(g_renderer, base_r, base_g, base_b, 255);
        SDL_RenderClear(g_renderer);
        
        free(flow_magnitudes);
        free(smoothed_r);
        free(smoothed_g);
        free(smoothed_b);
        return;
    }
    
    // Sample flow field and calculate water colors
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
    
    // Apply smoothing based on zoom level
    float smoothness = (zoom >= 0.3f) ? (0.6f + (zoom - 0.3f) * 1.5f) : (0.2f + zoom * 1.3f);
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
            
            // Calculate water color based on flow intensity
            int water_r, water_g, water_b;
            
            if (smoothed_flow < 0.1f) {
                // Very weak flow: clearer water
                water_r = base_r + (int)(8 * (0.1f - smoothed_flow) / 0.1f);
                water_g = base_g + (int)(12 * (0.1f - smoothed_flow) / 0.1f);
                water_b = base_b + (int)(15 * (0.1f - smoothed_flow) / 0.1f);
            } else if (smoothed_flow < 0.3f) {
                // Normal flow: base water color
                water_r = base_r;
                water_g = base_g;
                water_b = base_b;
            } else if (smoothed_flow < 0.6f) {
                // Medium flow: slightly darker
                float factor = (smoothed_flow - 0.3f) / 0.3f;
                water_r = base_r - (int)(6 * factor);
                water_g = base_g - (int)(5 * factor);
                water_b = base_b - (int)(8 * factor);
            } else {
                // Strong flow: much darker water
                float factor = (smoothed_flow - 0.6f) / 0.4f;
                water_r = base_r - 6 - (int)(8 * factor);
                water_g = base_g - 5 - (int)(3 * factor);
                water_b = base_b - 8 - (int)(12 * factor);
            }
            
            // Clamp colors to valid range
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
    
    // Render water tiles with flow-based colors
    for (int gy = 1; gy < grid_height - 1; gy++) {
        for (int gx = 1; gx < grid_width - 1; gx++) {
            float world_x = world_left + (gx - 1) * grid_size;
            float world_y = world_top + (gy - 1) * grid_size;
            
            int idx = gy * grid_width + gx;
            
            int screen_x1, screen_y1, screen_x2, screen_y2;
            camera_world_to_screen(world_x, world_y, &screen_x1, &screen_y1);
            camera_world_to_screen(world_x + grid_size, world_y + grid_size, &screen_x2, &screen_y2);
            
            // Only render if visible on screen
            if (screen_x2 > 0 && screen_x1 < WINDOW_WIDTH && 
                screen_y2 > 0 && screen_y1 < WINDOW_HEIGHT) {
                
                SDL_SetRenderDrawColor(g_renderer, smoothed_r[idx], smoothed_g[idx], smoothed_b[idx], 255);
                
                SDL_Rect water_rect = {screen_x1, screen_y1, screen_x2 - screen_x1, screen_y2 - screen_y1};
                SDL_RenderFillRect(g_renderer, &water_rect);
            }
        }
    }
    
    // Clean up memory
    free(flow_magnitudes);
    free(smoothed_r);
    free(smoothed_g);
    free(smoothed_b);
}

// Calculate aged color for plants based on maturity
static void calculate_aged_color(int base_r, int base_g, int base_b, int age, int age_mature, int* aged_r, int* aged_g, int* aged_b) {
    if (age_mature <= 0) age_mature = 1800;
    
    float age_factor = fminf((float)age / (float)age_mature, 1.0f);
    
    // Mix with brown color as plant ages
    int brown_r = 101, brown_g = 67, brown_b = 33;
    float brown_influence = age_factor * 0.20f;
    float original_influence = 1.0f - brown_influence;
    float darkness_factor = 1.0f - (age_factor * 0.08f);
    
    *aged_r = (int)((base_r * original_influence + brown_r * brown_influence) * darkness_factor);
    *aged_g = (int)((base_g * original_influence + brown_g * brown_influence) * darkness_factor);
    *aged_b = (int)((base_b * original_influence + brown_b * brown_influence) * darkness_factor);
    
    // Ensure minimum visibility
    if (*aged_r < 20) *aged_r = 20;
    if (*aged_g < 15) *aged_g = 15;
    if (*aged_b < 10) *aged_b = 10;
    
    if (*aged_r > 255) *aged_r = 255;
    if (*aged_g > 255) *aged_g = 255;
    if (*aged_b > 255) *aged_b = 255;
}

// Calculate bleached color for temperature-affected corals
static void calculate_bleached_color(int base_r, int base_g, int base_b, int* bleached_r, int* bleached_g, int* bleached_b) {
    // Convert to grayscale and make very light for bleached appearance
    int gray_value = (int)(0.299f * base_r + 0.587f * base_g + 0.114f * base_b);
    
    *bleached_r = (gray_value + 255) / 2;
    *bleached_g = (gray_value + 255) / 2;
    *bleached_b = (gray_value + 255) / 2;
    
    // Ensure high brightness for bleached effect
    if (*bleached_r < 200) *bleached_r = 200;
    if (*bleached_g < 200) *bleached_g = 200;
    if (*bleached_b < 200) *bleached_b = 200;
}

// Calculate corpse color based on original fish type and decay
static void calculate_corpse_color(int original_fish_type, int decay_timer, int* corpse_r, int* corpse_g, int* corpse_b) {
    int base_gray = 220;
    
    // Darken based on decay progress
    float decay_factor = (float)decay_timer / (float)CORPSE_DECAY_TIME;
    int gray_value = (int)(base_gray * (0.5f + decay_factor * 0.5f));
    
    // Apply subtle tint from original fish colors
    FishType* original_type = fish_get_type(original_fish_type);
    if (original_type) {
        *corpse_r = (gray_value * 9 + original_type->node_r) / 10;
        *corpse_g = (gray_value * 9 + original_type->node_g) / 10;
        *corpse_b = (gray_value * 9 + original_type->node_b) / 10;
    } else {
        *corpse_r = gray_value;
        *corpse_g = gray_value;
        *corpse_b = gray_value;
    }
    
    // Ensure visibility
    if (*corpse_r < 150) *corpse_r = 150;
    if (*corpse_g < 150) *corpse_g = 150;
    if (*corpse_b < 150) *corpse_b = 150;
}

// Draw thick line for rendering
static void draw_thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness) {
    if (thickness <= 1) {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    
    // Draw multiple parallel lines for thickness
    for (int i = -thickness/2; i <= thickness/2; i++) {
        SDL_RenderDrawLine(renderer, x1 + i, y1, x2 + i, y2);
        SDL_RenderDrawLine(renderer, x1, y1 + i, x2, y2 + i);
    }
}

// Enhanced fish tail rendering with configurable size
static void draw_enhanced_fish_tail(SDL_Renderer* renderer, int screen_x, int screen_y, float heading, FishType* fish_type, float base_radius, int r, int g, int b) {
    if (!fish_type) return;
    
    // Apply visual configuration factors
    float tail_length = base_radius * 2.5f * fish_type->tail_length_factor;
    float tail_width = base_radius * 3.0f * fish_type->tail_width_factor;
    
    // Calculate tail position (opposite to heading direction)
    float tail_offset_distance = base_radius * 3.0f;
    int tail_base_x = screen_x - (int)(cos(heading) * tail_offset_distance);
    int tail_base_y = screen_y - (int)(sin(heading) * tail_offset_distance);
    
    // Perpendicular vector for triangle width
    float perp_x = -sin(heading);
    float perp_y = cos(heading);
    
    // Calculate tail triangle points
    int tail_corner1_x = tail_base_x + (int)(perp_x * tail_width * 0.5f);
    int tail_corner1_y = tail_base_y + (int)(perp_y * tail_width * 0.5f);
    int tail_corner2_x = tail_base_x - (int)(perp_x * tail_width * 0.5f);
    int tail_corner2_y = tail_base_y - (int)(perp_y * tail_width * 0.5f);
    int tail_tip_x = tail_base_x + (int)(cos(heading) * tail_length);
    int tail_tip_y = tail_base_y + (int)(sin(heading) * tail_length);
    
    // Use same color as fish body
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    
    // Draw tail triangle outline
    SDL_RenderDrawLine(renderer, tail_base_x, tail_base_y, tail_corner1_x, tail_corner1_y);
    SDL_RenderDrawLine(renderer, tail_corner1_x, tail_corner1_y, tail_tip_x, tail_tip_y);
    SDL_RenderDrawLine(renderer, tail_tip_x, tail_tip_y, tail_corner2_x, tail_corner2_y);
    SDL_RenderDrawLine(renderer, tail_corner2_x, tail_corner2_y, tail_base_x, tail_base_y);
    
    // Fill tail triangle for larger tails
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

// Draw fish RL vision rays for debugging
static void draw_fish_rl_vision(SDL_Renderer* renderer, int fish_id) {
    if (!fish_is_ray_rendering_enabled()) return;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    
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
    
    // Draw target detection vector if present
    float target_vec_x = fish->rl_inputs[0];
    float target_vec_y = fish->rl_inputs[1];
    
    if (target_vec_x != 0.0f || target_vec_y != 0.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 200);
        
        float target_range = 150.0f;
        int target_end_x = fish_screen_x + (int)(target_vec_x * target_range * camera_get_zoom());
        int target_end_y = fish_screen_y + (int)(target_vec_y * target_range * camera_get_zoom());
        
        // Draw thick arrow to target
        for (int i = -2; i <= 2; i++) {
            SDL_RenderDrawLine(renderer, fish_screen_x + i, fish_screen_y, target_end_x + i, target_end_y);
            SDL_RenderDrawLine(renderer, fish_screen_x, fish_screen_y + i, target_end_x, target_end_y + i);
        }
        
        // Draw arrowhead
        float arrow_angle = atan2(target_vec_y, target_vec_x);
        float arrow_size = 10.0f * camera_get_zoom();
        
        int arrow1_x = target_end_x - (int)(cos(arrow_angle - 0.5f) * arrow_size);
        int arrow1_y = target_end_y - (int)(sin(arrow_angle - 0.5f) * arrow_size);
        int arrow2_x = target_end_x - (int)(cos(arrow_angle + 0.5f) * arrow_size);
        int arrow2_y = target_end_y - (int)(sin(arrow_angle + 0.5f) * arrow_size);
        
        SDL_RenderDrawLine(renderer, target_end_x, target_end_y, arrow1_x, arrow1_y);
        SDL_RenderDrawLine(renderer, target_end_x, target_end_y, arrow2_x, arrow2_y);
    }
    
    // Draw heading direction indicator
    SDL_SetRenderDrawColor(renderer, 255, 255, 100, 150);
    
    int heading_end_x = fish_screen_x + (int)(cos(fish->heading) * 50.0f * camera_get_zoom());
    int heading_end_y = fish_screen_y + (int)(sin(fish->heading) * 50.0f * camera_get_zoom());
    
    draw_thick_line(renderer, fish_screen_x, fish_screen_y, heading_end_x, heading_end_y, 3);
}

// Enhanced curved line drawing for plant chains
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
    
    // Perpendicular vector for curve
    float perp_x = -dy;
    float perp_y = dx;
    
    float curve_amount = curve_strength * length * 0.3f + curve_offset;
    float ctrl_x = mid_x + perp_x * curve_amount;
    float ctrl_y = mid_y + perp_y * curve_amount;
    
    // Draw curved line using segments
    int segments = (int)(length / 8.0f) + 3;
    if (segments > 20) segments = 20;
    
    float prev_x = x1;
    float prev_y = y1;
    
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / segments;
        float inv_t = 1.0f - t;
        
        // Quadratic Bezier curve
        float curr_x = inv_t * inv_t * x1 + 2 * inv_t * t * ctrl_x + t * t * x2;
        float curr_y = inv_t * inv_t * y1 + 2 * inv_t * t * ctrl_y + t * t * y2;
        
        draw_thick_line(renderer, (int)prev_x, (int)prev_y, (int)curr_x, (int)curr_y, thickness);
        
        prev_x = curr_x;
        prev_y = curr_y;
    }
}

// FPS display functions
void rendering_update_fps(float fps) {
    g_current_fps = fps;
    snprintf(g_fps_text, sizeof(g_fps_text), "%.1f", fps);
}

void rendering_draw_fps(void) {
    if (!g_renderer) return;
    
    // Draw semi-transparent background
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 128);
    SDL_Rect bg_rect = {WINDOW_WIDTH - 45, 10, 35, 12};
    SDL_RenderFillRect(g_renderer, &bg_rect);
    
    // Draw FPS text
    draw_simple_text(g_renderer, g_fps_text, WINDOW_WIDTH - 42, 12, 1);
}

int rendering_init(SDL_Renderer* renderer) {
    g_renderer = renderer;
    
    // Set renderer for all layer systems
    nutrition_set_renderer(renderer);
    gas_set_renderer(renderer);
    flow_set_renderer(renderer);
    
    // Initialize FPS display
    strcpy(g_fps_text, "0.0");
    
    printf("Enhanced rendering system initialized with configurable fish visuals\n");
    return 1;
}

void rendering_cleanup(void) {
    g_renderer = NULL;
    printf("Rendering system cleaned up\n");
}

// Main rendering function with enhanced fish visualization
void rendering_render(void) {
    if (!g_renderer) return;
    
    // Render flow-based water background
    render_flow_based_water_background();
    
    // Render environmental layers
    nutrition_render();
    gas_render();
    flow_render();
    
    // Get viewport bounds for culling
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    Node* nodes = simulation_get_nodes();
    Chain* chains = simulation_get_chains();
    int node_count = simulation_get_node_count();
    int chain_count = simulation_get_chain_count();
    int selected_node = simulation_get_selected_node();
    int selection_mode = simulation_get_selection_mode();
    
    // Render plant chains with enhanced visual properties
    for (int i = 0; i < chain_count; i++) {
        if (!chains[i].active) continue;
        
        int n1 = chains[i].node1;
        int n2 = chains[i].node2;
        
        if (n1 < 0 || n1 >= node_count || n2 < 0 || n2 >= node_count) continue;
        if (!nodes[n1].active || !nodes[n2].active) continue;
        
        // Skip chains involving fish nodes or corpses
        if (nodes[n1].plant_type == -1 || nodes[n2].plant_type == -1) continue;
        if (nodes[n1].is_corpse || nodes[n2].is_corpse) continue;
        
        // Frustum culling for performance
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
        
        // Calculate chain color with aging and bleaching effects
        if (pt && pt->active) {
            int aged_r, aged_g, aged_b;
            calculate_aged_color(pt->chain_r, pt->chain_g, pt->chain_b, chains[i].age, pt->age_mature, &aged_r, &aged_g, &aged_b);
            
            // Check for coral bleaching
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
        
        // Convert world coordinates to screen coordinates
        int screen_x1, screen_y1, screen_x2, screen_y2;
        camera_world_to_screen(nodes[n1].x, nodes[n1].y, &screen_x1, &screen_y1);
        camera_world_to_screen(nodes[n2].x, nodes[n2].y, &screen_x2, &screen_y2);
        
        // Calculate thickness with plant-specific factor
        float thickness_factor = pt ? pt->chain_thickness_factor : 1.0f;
        int thickness = (int)(CHAIN_THICKNESS * camera_get_zoom() * thickness_factor);
        if (thickness < 2) thickness = 2;
        
        // Apply curvature with plant-specific multiplier
        float final_curve_strength = chains[i].curve_strength * chains[i].curve_multiplier;
        
        draw_curved_line(g_renderer, screen_x1, screen_y1, screen_x2, screen_y2, 
                        final_curve_strength, chains[i].curve_offset, thickness);
    }
    
    // Render fish RL vision rays BEFORE fish bodies for proper layering
    if (fish_is_ray_rendering_enabled()) {
        Fish* all_fish = fish_get_all();
        int highest_slot = fish_get_highest_slot();
        
        for (int i = 0; i <= highest_slot && i < MAX_FISH; i++) {
            if (all_fish[i].active) {
                draw_fish_rl_vision(g_renderer, i);
            }
        }
    }
    
    // Render all nodes (plants, fish, corpses) with enhanced visual properties
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
        
        // Render corpse nodes with scaled size based on original fish type
        if (nodes[i].is_corpse) {
            if (scaled_radius < 1) scaled_radius = 1;
            
            int corpse_r, corpse_g, corpse_b;
            calculate_corpse_color(nodes[i].original_fish_type, nodes[i].corpse_decay_timer, &corpse_r, &corpse_g, &corpse_b);
            
            // Get original fish type for scaling
            FishType* original_fish_type = fish_get_type(nodes[i].original_fish_type);
            float corpse_size_factor = original_fish_type ? original_fish_type->node_size_factor : 1.0f;
            
            // Apply fish size scaling to corpse (same as living fish size)
            scaled_radius = (int)((NODE_RADIUS * 1.5f * corpse_size_factor) * camera_get_zoom());
            if (scaled_radius < 1) scaled_radius = 1;
            
            // Draw corpse tail first with proper scaling
            if (scaled_radius > 2 && original_fish_type) {
                draw_enhanced_fish_tail(g_renderer, screen_x, screen_y, nodes[i].corpse_heading, 
                                       original_fish_type, scaled_radius, corpse_r, corpse_g, corpse_b);
            }
            
            SDL_SetRenderDrawColor(g_renderer, corpse_r, corpse_g, corpse_b, 255);
            
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
                // Draw filled circle
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
            
            // Draw decay indicator for heavily decayed corpses
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
        
        // Render fish nodes with enhanced visual configuration
        if (nodes[i].plant_type == -1) {
            // Find fish that owns this node
            Fish* fish = find_fish_by_node_id(i);
            
            if (!fish) {
                // Skip rendering if no fish found (prevents orphaned nodes)
                continue;
            }
            
            FishType* fish_type = fish_get_type(fish->fish_type);
            int fish_r = 255, fish_g = 165, fish_b = 0;
            
            if (fish_type && fish_type->active) {
                // Apply fish visual configuration with scaling
                scaled_radius = (int)((NODE_RADIUS * 1.8f * fish_type->node_size_factor) * camera_get_zoom());
                if (scaled_radius < 1) scaled_radius = 1;
                
                // Use fish type colors
                fish_r = fish_type->node_r;
                fish_g = fish_type->node_g;
                fish_b = fish_type->node_b;
            } else {
                scaled_radius = (int)((NODE_RADIUS * 1.8f) * camera_get_zoom());
                if (scaled_radius < 1) scaled_radius = 1;
            }
            
            SDL_SetRenderDrawColor(g_renderer, fish_r, fish_g, fish_b, 255);
            
            // Draw enhanced fish tail first (behind body)
            if (scaled_radius > 2 && fish_type) {
                draw_enhanced_fish_tail(g_renderer, screen_x, screen_y, fish->heading, 
                                       fish_type, scaled_radius, fish_r, fish_g, fish_b);
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
                // Draw filled circle for fish body
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
        } 
        
        // Render plant nodes with enhanced visual effects
        else {
            int plant_type = nodes[i].plant_type;
            PlantType* pt = plants_get_type(plant_type);
            
            // Apply plant size configuration
            float size_factor = pt ? pt->node_size_factor : 1.0f;
            scaled_radius = (int)(NODE_RADIUS * camera_get_zoom() * size_factor);
            if (scaled_radius < 1) scaled_radius = 1;
            
            // Set color based on selection, aging, bleaching, and seed immunity
            if (i == selected_node && selection_mode == 1) {
                SDL_SetRenderDrawColor(g_renderer, 255, 255, 0, 255);
            } else {
                if (pt && pt->active) {
                    int aged_r, aged_g, aged_b;
                    calculate_aged_color(pt->node_r, pt->node_g, pt->node_b, nodes[i].age, pt->age_mature, &aged_r, &aged_g, &aged_b);
                    
                    // Check for coral bleaching
                    if (temperature_is_coral_bleached(i)) {
                        int bleached_r, bleached_g, bleached_b;
                        calculate_bleached_color(aged_r, aged_g, aged_b, &bleached_r, &bleached_g, &bleached_b);
                        SDL_SetRenderDrawColor(g_renderer, bleached_r, bleached_g, bleached_b, 255);
                    } 
                    // Check for seed immunity (pulsing effect)
                    else if (nodes[i].seed_immunity_timer > 0) {
                        float immunity_ratio = (float)nodes[i].seed_immunity_timer / (float)SEED_IMMUNITY_TIME;
                        
                        // Pulsing effect based on frame counter
                        int pulse_frame = simulation_get_frame_counter() % 60;
                        float pulse_factor = 0.7f + 0.3f * sin((pulse_frame / 60.0f) * 2.0f * M_PI);
                        
                        // Bright pulsing color for immune seeds
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
        
        // Draw plant node as filled circle
        if (scaled_radius <= 2) {
            SDL_RenderDrawPoint(g_renderer, screen_x, screen_y);
            if (scaled_radius > 1) {
                SDL_RenderDrawPoint(g_renderer, screen_x-1, screen_y);
                SDL_RenderDrawPoint(g_renderer, screen_x+1, screen_y);
                SDL_RenderDrawPoint(g_renderer, screen_x, screen_y-1);
                SDL_RenderDrawPoint(g_renderer, screen_x, screen_y+1);
            }
        } else {
            // Draw filled circle
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
    
    // Draw FPS display in top-right corner
    rendering_draw_fps();
    
    // Present the rendered frame
    SDL_RenderPresent(g_renderer);
}