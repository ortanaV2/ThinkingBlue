#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "flow.h"
#include "camera.h"

// Clean arrow parameters - bigger but simple
#define FLOW_ARROW_SIZE 30.0f        // Larger arrows for visibility
#define FLOW_ARROW_SPACING 70.0f     // Good spacing
#define FLOW_MAX_MAGNITUDE 3.0f

static float* g_flow_x = NULL;
static float* g_flow_y = NULL;
static int g_grid_width = 0;
static int g_grid_height = 0;
static int g_visible = 0;
static SDL_Renderer* g_renderer = NULL;

// Perlin noise permutation table
static int p[512];
static int permutation[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static void init_perlin(void) {
    for (int i = 0; i < 256; i++) {
        p[i] = permutation[i];
        p[256 + i] = permutation[i];
    }
}

static float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static float grad(int hash, float x, float y) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : 0;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

static float perlin_noise(float x, float y) {
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;
    
    x -= floor(x);
    y -= floor(y);
    
    float u = fade(x);
    float v = fade(y);
    
    int A = p[X] + Y;
    int AA = p[A];
    int AB = p[A + 1];
    int B = p[X + 1] + Y;
    int BA = p[B];
    int BB = p[B + 1];
    
    return lerp(v, 
                lerp(u, grad(p[AA], x, y),
                        grad(p[BA], x - 1, y)),
                lerp(u, grad(p[AB], x, y - 1),
                        grad(p[BB], x - 1, y - 1)));
}

static float octave_perlin(float x, float y, int octaves, float persistence, float scale) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = scale;
    float max_value = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += perlin_noise(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    
    return value / max_value;
}

static void generate_flow_field(void) {
    unsigned int seed = (unsigned int)time(NULL);
    seed ^= (unsigned int)clock();
    seed ^= (unsigned int)getpid();
    seed += (unsigned int)((uintptr_t)&seed);
    srand(seed);
    
    // Shuffle permutation table for unique field
    for (int i = 0; i < 256; i++) {
        int j = rand() % 256;
        int temp = permutation[i];
        permutation[i] = permutation[j];
        permutation[j] = temp;
    }
    
    init_perlin();
    
    // Multiple offset layers for complex flow patterns
    float offset_x[6], offset_y[6];
    for (int i = 0; i < 6; i++) {
        offset_x[i] = ((float)rand() / RAND_MAX) * 5000.0f;
        offset_y[i] = ((float)rand() / RAND_MAX) * 5000.0f;
    }
    
    for (int y = 0; y < g_grid_height; y++) {
        for (int x = 0; x < g_grid_width; x++) {
            int index = y * g_grid_width + x;
            
            // Base flow pattern - large scale circulation
            float px1 = (x + offset_x[0]) * 0.008f;
            float py1 = (y + offset_y[0]) * 0.008f;
            float base_angle = octave_perlin(px1, py1, 3, 0.6f, 1.0f) * 2.0f * M_PI;
            float base_strength = 0.4f + octave_perlin(px1 + 1000, py1 + 1000, 2, 0.5f, 1.0f) * 0.3f;
            
            float flow_x = cos(base_angle) * base_strength;
            float flow_y = sin(base_angle) * base_strength;
            
            // Medium scale turbulence
            float px2 = (x + offset_x[1]) * 0.02f;
            float py2 = (y + offset_y[1]) * 0.02f;
            float turb_angle = octave_perlin(px2, py2, 4, 0.5f, 1.0f) * M_PI;
            float turb_strength = octave_perlin(px2 + 2000, py2 + 2000, 3, 0.4f, 1.0f) * 0.8f;
            
            flow_x += cos(turb_angle) * turb_strength;
            flow_y += sin(turb_angle) * turb_strength;
            
            // Fine scale eddies and vortices
            float px3 = (x + offset_x[2]) * 0.05f;
            float py3 = (y + offset_y[2]) * 0.05f;
            float eddy_angle = octave_perlin(px3, py3, 2, 0.7f, 1.0f) * M_PI * 0.5f;
            float eddy_strength = octave_perlin(px3 + 3000, py3 + 3000, 2, 0.6f, 1.0f) * 0.6f;
            
            flow_x += cos(eddy_angle) * eddy_strength;
            flow_y += sin(eddy_angle) * eddy_strength;
            
            // Directional current bias (creates overall flow direction)
            float px4 = (x + offset_x[3]) * 0.003f;
            float py4 = (y + offset_y[3]) * 0.003f;
            float current_bias = octave_perlin(px4, py4, 2, 0.8f, 1.0f);
            
            // Create spiral/vortex patterns
            float center_x = g_grid_width * 0.5f;
            float center_y = g_grid_height * 0.5f;
            float dist_to_center = sqrt((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
            float spiral_angle = atan2(y - center_y, x - center_x) + dist_to_center * 0.01f;
            float spiral_strength = 0.3f * exp(-dist_to_center * 0.002f);
            
            flow_x += cos(spiral_angle) * spiral_strength * current_bias;
            flow_y += sin(spiral_angle) * spiral_strength * current_bias;
            
            // Add some random vortices at specific locations
            for (int v = 0; v < 3; v++) {
                float vortex_x = offset_x[4 + (v % 2)] * 0.0003f * g_grid_width;
                float vortex_y = offset_y[4 + (v % 2)] * 0.0003f * g_grid_height;
                float vortex_dist = sqrt((x - vortex_x) * (x - vortex_x) + (y - vortex_y) * (y - vortex_y));
                
                if (vortex_dist > 0.1f) {
                    float vortex_angle = atan2(y - vortex_y, x - vortex_x) + M_PI * 0.5f;
                    float vortex_strength = 1.5f * exp(-vortex_dist * 0.01f);
                    
                    flow_x += cos(vortex_angle) * vortex_strength;
                    flow_y += sin(vortex_angle) * vortex_strength;
                }
            }
            
            // Boundary effects - create walls and channels
            float boundary_factor = 1.0f;
            float border_dist = fminf(fminf(x, g_grid_width - x), fminf(y, g_grid_height - y));
            if (border_dist < 20) {
                boundary_factor = border_dist / 20.0f;
            }
            
            flow_x *= boundary_factor;
            flow_y *= boundary_factor;
            
            // Clamp to maximum magnitude
            float magnitude = sqrt(flow_x * flow_x + flow_y * flow_y);
            if (magnitude > FLOW_MAX_MAGNITUDE) {
                flow_x = (flow_x / magnitude) * FLOW_MAX_MAGNITUDE;
                flow_y = (flow_y / magnitude) * FLOW_MAX_MAGNITUDE;
            }
            
            g_flow_x[index] = flow_x;
            g_flow_y[index] = flow_y;
        }
    }
    
    printf("Generated complex flow field with vortices and turbulence\n");
}

int flow_init(void) {
    g_grid_width = (int)ceil(WORLD_WIDTH / LAYER_GRID_SIZE);
    g_grid_height = (int)ceil(WORLD_HEIGHT / LAYER_GRID_SIZE);
    
    g_flow_x = malloc(g_grid_width * g_grid_height * sizeof(float));
    g_flow_y = malloc(g_grid_width * g_grid_height * sizeof(float));
    
    if (!g_flow_x || !g_flow_y) {
        printf("Failed to allocate flow field\n");
        return 0;
    }
    
    generate_flow_field();
    
    g_visible = 0;
    
    printf("Flow field initialized: %dx%d grid (%.1f unit cells)\n", 
           g_grid_width, g_grid_height, LAYER_GRID_SIZE);
    return 1;
}

void flow_cleanup(void) {
    if (g_flow_x) {
        free(g_flow_x);
        g_flow_x = NULL;
    }
    if (g_flow_y) {
        free(g_flow_y);
        g_flow_y = NULL;
    }
}

void flow_set_renderer(SDL_Renderer* renderer) {
    g_renderer = renderer;
}

void flow_toggle_visibility(void) {
    g_visible = !g_visible;
    printf("Flow field: %s\n", g_visible ? "ON" : "OFF");
}

int flow_is_visible(void) {
    return g_visible;
}

static void world_to_flow_grid(float world_x, float world_y, int* grid_x, int* grid_y) {
    *grid_x = (int)floor((world_x - WORLD_LEFT) / LAYER_GRID_SIZE);
    *grid_y = (int)floor((world_y - WORLD_TOP) / LAYER_GRID_SIZE);
}

void flow_get_vector_at(float world_x, float world_y, float* flow_x, float* flow_y) {
    int grid_x, grid_y;
    world_to_flow_grid(world_x, world_y, &grid_x, &grid_y);
    
    if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
        *flow_x = 0.0f;
        *flow_y = 0.0f;
        return;
    }
    
    int index = grid_y * g_grid_width + grid_x;
    *flow_x = g_flow_x[index];
    *flow_y = g_flow_y[index];
}

float flow_get_magnitude_at(float world_x, float world_y) {
    float flow_x, flow_y;
    flow_get_vector_at(world_x, world_y, &flow_x, &flow_y);
    return sqrt(flow_x * flow_x + flow_y * flow_y);
}

// Simple, clean arrow drawing - like original but thicker and more visible
static void draw_arrow(SDL_Renderer* renderer, int start_x, int start_y, float dir_x, float dir_y, float magnitude) {
    if (magnitude < 0.05f) return;
    
    // Scale arrow size based on magnitude and zoom
    float zoom = camera_get_zoom();
    float arrow_size = FLOW_ARROW_SIZE * zoom * (magnitude / FLOW_MAX_MAGNITUDE);
    if (arrow_size < 10.0f) arrow_size = 10.0f;  // Good minimum size
    if (arrow_size > 60.0f) arrow_size = 60.0f;  // Reasonable maximum
    
    // Normalize direction
    float length = sqrt(dir_x * dir_x + dir_y * dir_y);
    if (length < 0.01f) return;
    
    dir_x /= length;
    dir_y /= length;
    
    // Arrow end point
    int end_x = start_x + (int)(dir_x * arrow_size);
    int end_y = start_y + (int)(dir_y * arrow_size);
    
    // High contrast colors for visibility
    float norm_magnitude = magnitude / FLOW_MAX_MAGNITUDE;
    int r = (int)(norm_magnitude * 255);
    int g = (int)((1.0f - norm_magnitude) * 150 + 100);  // Bright green-yellow range
    int b = (int)((1.0f - norm_magnitude) * 255);
    
    // Ensure high visibility
    if (r < 100) r = 100;
    if (g < 80) g = 80;
    if (b < 100) b = 100;
    
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    
    // Draw thicker main arrow shaft (3 parallel lines)
    SDL_RenderDrawLine(renderer, start_x, start_y, end_x, end_y);
    SDL_RenderDrawLine(renderer, start_x + 1, start_y, end_x + 1, end_y);
    SDL_RenderDrawLine(renderer, start_x - 1, start_y, end_x - 1, end_y);
    SDL_RenderDrawLine(renderer, start_x, start_y + 1, end_x, end_y + 1);
    SDL_RenderDrawLine(renderer, start_x, start_y - 1, end_x, end_y - 1);
    
    // Draw simple, clean arrowhead (like original but thicker)
    if (arrow_size > 8.0f) {
        float head_size = arrow_size * 0.3f;
        
        // Perpendicular vector for arrowhead
        float perp_x = -dir_y;
        float perp_y = dir_x;
        
        // Arrowhead points
        int head_x1 = end_x - (int)(dir_x * head_size + perp_x * head_size * 0.5f);
        int head_y1 = end_y - (int)(dir_y * head_size + perp_y * head_size * 0.5f);
        int head_x2 = end_x - (int)(dir_x * head_size - perp_x * head_size * 0.5f);
        int head_y2 = end_y - (int)(dir_y * head_size - perp_y * head_size * 0.5f);
        
        // Draw arrowhead with thickness (3 parallel lines each)
        SDL_RenderDrawLine(renderer, end_x, end_y, head_x1, head_y1);
        SDL_RenderDrawLine(renderer, end_x + 1, end_y, head_x1 + 1, head_y1);
        SDL_RenderDrawLine(renderer, end_x - 1, end_y, head_x1 - 1, head_y1);
        
        SDL_RenderDrawLine(renderer, end_x, end_y, head_x2, head_y2);
        SDL_RenderDrawLine(renderer, end_x + 1, end_y, head_x2 + 1, head_y2);
        SDL_RenderDrawLine(renderer, end_x - 1, end_y, head_x2 - 1, head_y2);
    }
}

void flow_render(void) {
    if (!g_visible || !g_renderer || !g_flow_x || !g_flow_y) return;
    
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    float zoom = camera_get_zoom();
    float arrow_spacing = FLOW_ARROW_SPACING / zoom;
    if (arrow_spacing < 25.0f) arrow_spacing = 25.0f;
    if (arrow_spacing > 120.0f) arrow_spacing = 120.0f;
    
    // Calculate which arrows to draw based on viewport
    float start_world_x = world_left;
    float end_world_x = world_right;
    float start_world_y = world_top;
    float end_world_y = world_bottom;
    
    for (float world_y = start_world_y; world_y <= end_world_y; world_y += arrow_spacing) {
        for (float world_x = start_world_x; world_x <= end_world_x; world_x += arrow_spacing) {
            int grid_x, grid_y;
            world_to_flow_grid(world_x, world_y, &grid_x, &grid_y);
            
            if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
                continue;
            }
            
            int index = grid_y * g_grid_width + grid_x;
            float flow_x = g_flow_x[index];
            float flow_y = g_flow_y[index];
            float magnitude = sqrt(flow_x * flow_x + flow_y * flow_y);
            
            int screen_x, screen_y;
            camera_world_to_screen(world_x, world_y, &screen_x, &screen_y);
            
            // Only draw if on screen
            if (screen_x >= -50 && screen_x <= WINDOW_WIDTH + 50 &&
                screen_y >= -50 && screen_y <= WINDOW_HEIGHT + 50) {
                draw_arrow(g_renderer, screen_x, screen_y, flow_x, flow_y, magnitude);
            }
        }
    }
}