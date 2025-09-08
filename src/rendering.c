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

static void draw_thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness);
static void draw_curved_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, float curve_strength, float curve_offset, int thickness);
static void draw_fish_tail(SDL_Renderer* renderer, int screen_x, int screen_y, float vx, float vy, int fish_radius, int r, int g, int b);
static void draw_fish_vision_rays(SDL_Renderer* renderer, int fish_id);

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

static void draw_fish_tail(SDL_Renderer* renderer, int screen_x, int screen_y, float vx, float vy, int fish_radius, int r, int g, int b) {
    // Calculate tail direction (same as movement direction - 180° rotated)
    float speed = sqrt(vx * vx + vy * vy);
    if (speed < 0.1f) {
        // If not moving, point tail to the right
        vx = 1.0f;
        vy = 0.0f;
        speed = 1.0f;
    }
    
    // Normalize movement vector (now pointing in movement direction)
    float dir_x = vx / speed; // Same direction as movement
    float dir_y = vy / speed;
    
    // === TAIL POSITION OFFSET ===
    // Change these values to move the tail relative to the fish node
    float tail_offset_distance = fish_radius * 3.0f; // How far behind/in front of fish center
    
    // Calculate tail base position (offset from fish center)
    int tail_base_x = screen_x - (int)(dir_x * tail_offset_distance); // Negative = behind fish
    int tail_base_y = screen_y - (int)(dir_y * tail_offset_distance);
    
    // Tail size - make it much larger
    float tail_length = fish_radius * 2.5f; // 3.5x fish radius (much bigger)
    float tail_width = fish_radius * 3.0f;  // 2x fish radius (wider)
    
    // The tail base is one corner of the triangle
    // Calculate the other two corners
    
    // Perpendicular vector for triangle width
    float perp_x = -dir_y;
    float perp_y = dir_x;
    
    // Two tail corners - positioned around the tail base
    int tail_corner1_x = tail_base_x + (int)(perp_x * tail_width * 0.5f);
    int tail_corner1_y = tail_base_y + (int)(perp_y * tail_width * 0.5f);
    int tail_corner2_x = tail_base_x - (int)(perp_x * tail_width * 0.5f);
    int tail_corner2_y = tail_base_y - (int)(perp_y * tail_width * 0.5f);
    
    // Third corner (tip of tail) - extends forward in movement direction
    int tail_tip_x = tail_base_x + (int)(dir_x * tail_length);
    int tail_tip_y = tail_base_y + (int)(dir_y * tail_length);
    
    // Make tail color slightly darker
    int tail_r = (r * 3) / 4;
    int tail_g = (g * 3) / 4;
    int tail_b = (b * 3) / 4;
    SDL_SetRenderDrawColor(renderer, tail_r, tail_g, tail_b, 255);
    
    // Draw triangle outline
    // Triangle: tail_base -> corner1 -> tip -> corner2 -> tail_base
    SDL_RenderDrawLine(renderer, tail_base_x, tail_base_y, tail_corner1_x, tail_corner1_y);
    SDL_RenderDrawLine(renderer, tail_corner1_x, tail_corner1_y, tail_tip_x, tail_tip_y);
    SDL_RenderDrawLine(renderer, tail_tip_x, tail_tip_y, tail_corner2_x, tail_corner2_y);
    SDL_RenderDrawLine(renderer, tail_corner2_x, tail_corner2_y, tail_base_x, tail_base_y);
    
    // Fill triangle using scanline algorithm
    if (tail_width > 2.0f) {
        // Simple triangle filling by drawing lines from tail base to opposite edge
        for (int i = 0; i <= (int)(tail_length); i++) {
            float t = (float)i / tail_length;
            
            // Points along the opposite edge (from corner1 to corner2)
            int edge_x1 = tail_corner1_x + (int)(t * (tail_tip_x - tail_corner1_x));
            int edge_y1 = tail_corner1_y + (int)(t * (tail_tip_y - tail_corner1_y));
            int edge_x2 = tail_corner2_x + (int)(t * (tail_tip_x - tail_corner2_x));
            int edge_y2 = tail_corner2_y + (int)(t * (tail_tip_y - tail_corner2_y));
            
            // Draw line from tail base to opposite edge
            SDL_RenderDrawLine(renderer, tail_base_x, tail_base_y, edge_x1, edge_y1);
            SDL_RenderDrawLine(renderer, tail_base_x, tail_base_y, edge_x2, edge_y2);
            
            // Also draw the edge itself
            SDL_RenderDrawLine(renderer, edge_x1, edge_y1, edge_x2, edge_y2);
        }
    }
}

static void draw_fish_vision_rays(SDL_Renderer* renderer, int fish_id) {
    if (!fish_is_ray_rendering_enabled()) return;
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) return;
    
    Node* nodes = simulation_get_nodes();
    Node* fish_node = &nodes[fish->node_id];
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) return;
    
    // Calculate fish heading
    float heading = 0.0f;
    if (fabs(fish_node->vx) > 0.01f || fabs(fish_node->vy) > 0.01f) {
        heading = atan2(fish_node->vy, fish_node->vx);
    }
    
    // Draw each vision ray
    float half_fov = fish_type->fov_angle * 0.5f;
    for (int i = 0; i < 8; i++) {
        float ray_angle = heading - half_fov + (fish_type->fov_angle * i / 7.0f);
        float ray_distance = fish_get_vision_ray(fish_id, i) * fish_type->fov_range;
        
        // Calculate ray end point
        float end_x = fish_node->x + cos(ray_angle) * ray_distance;
        float end_y = fish_node->y + sin(ray_angle) * ray_distance;
        
        // Convert to screen coordinates
        int fish_screen_x, fish_screen_y;
        int end_screen_x, end_screen_y;
        camera_world_to_screen(fish_node->x, fish_node->y, &fish_screen_x, &fish_screen_y);
        camera_world_to_screen(end_x, end_y, &end_screen_x, &end_screen_y);
        
        // Color based on ray hit distance (red = close obstacle, green = clear)
        float normalized_distance = fish_get_vision_ray(fish_id, i);
        int ray_r = (int)((1.0f - normalized_distance) * 255); // Red for obstacles
        int ray_g = (int)(normalized_distance * 255);          // Green for clear
        int ray_b = 50;                                        // Low blue
        
        SDL_SetRenderDrawColor(renderer, ray_r, ray_g, ray_b, 150); // Semi-transparent
        SDL_RenderDrawLine(renderer, fish_screen_x, fish_screen_y, end_screen_x, end_screen_y);
        
        // Draw small circle at ray end
        for (int dx = -2; dx <= 2; dx++) {
            for (int dy = -2; dy <= 2; dy++) {
                if (dx*dx + dy*dy <= 4) {
                    SDL_RenderDrawPoint(renderer, end_screen_x + dx, end_screen_y + dy);
                }
            }
        }
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
    
    // Render fish vision rays FIRST (behind fish)
    if (fish_is_ray_rendering_enabled()) {
        for (int i = 0; i < fish_count; i++) {
            if (fish_list[i].active) {
                draw_fish_vision_rays(g_renderer, i);
            }
        }
    }
    
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
            
            int fish_r = 255, fish_g = 165, fish_b = 0; // Default orange
            
            if (fish) {
                FishType* fish_type = fish_get_type(fish->fish_type);
                if (fish_type) {
                    // Use SAME scaling as plants, just multiply base NODE_RADIUS
                    scaled_radius = (int)((NODE_RADIUS * 1.8f) * camera_get_zoom()); // 1.8x larger than plants
                    if (scaled_radius < 1) scaled_radius = 1; // Same minimum as plants
                    
                    fish_r = fish_type->node_r;
                    fish_g = fish_type->node_g;
                    fish_b = fish_type->node_b;
                    SDL_SetRenderDrawColor(g_renderer, fish_r, fish_g, fish_b, 255);
                } else {
                    scaled_radius = (int)((NODE_RADIUS * 1.8f) * camera_get_zoom()); // Fallback size
                    if (scaled_radius < 1) scaled_radius = 1;
                    SDL_SetRenderDrawColor(g_renderer, fish_r, fish_g, fish_b, 255); // Orange fallback for fish
                }
            } else {
                scaled_radius = (int)((NODE_RADIUS * 1.8f) * camera_get_zoom()); // Fallback size
                if (scaled_radius < 1) scaled_radius = 1;
                fish_r = 255; fish_g = 0; fish_b = 255; // Magenta for orphaned fish nodes
                SDL_SetRenderDrawColor(g_renderer, fish_r, fish_g, fish_b, 255);
            }
            
            // Draw fish tail FIRST (under the node)
            if (scaled_radius > 2) {
                draw_fish_tail(g_renderer, screen_x, screen_y, nodes[i].vx, nodes[i].vy, scaled_radius, fish_r, fish_g, fish_b);
            }
            
            // Draw fish body (circle) SECOND (over the tail)
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
            
            continue; // Skip normal plant rendering
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