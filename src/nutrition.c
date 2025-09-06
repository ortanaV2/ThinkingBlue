#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "nutrition.h"
#include "camera.h"

#define NUTRITION_GRID_SIZE 35.0f
#define NUTRITION_SMOOTHNESS 10.0f

static float* g_nutrition_grid = NULL;
static float* g_original_nutrition = NULL; // Store original values to prevent terrain destruction
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

static void generate_perlin_terrain(void) {
    init_perlin();
    
    for (int y = 0; y < g_grid_height; y++) {
        for (int x = 0; x < g_grid_width; x++) {
            float value = 0.0f;
            
            value += octave_perlin(x, y, 4, 0.6f, 0.005f) * 1.0f;
            value += octave_perlin(x, y, 6, 0.5f, 0.02f) * 0.4f;
            value += octave_perlin(x, y, 4, 0.4f, 0.08f) * 0.3f;
            value += octave_perlin(x, y, 2, 0.3f, 0.2f) * 0.2f;
            
            value = (value + 1.0f) * 0.5f;
            value += ((float)rand() / RAND_MAX - 0.5f) * 0.3f;
            value = (value - 0.5f) * 2.0f + 0.5f;
            
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            
            g_nutrition_grid[y * g_grid_width + x] = value;
        }
    }
    
    float min_val = 1.0f, max_val = 0.0f;
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        if (g_nutrition_grid[i] < min_val) min_val = g_nutrition_grid[i];
        if (g_nutrition_grid[i] > max_val) max_val = g_nutrition_grid[i];
    }
    
    float range = max_val - min_val;
    if (range > 0.0f) {
        for (int i = 0; i < g_grid_width * g_grid_height; i++) {
            g_nutrition_grid[i] = (g_nutrition_grid[i] - min_val) / range;
        }
    }
}

static void apply_smoothing(void) {
    float* temp_grid = malloc(g_grid_width * g_grid_height * sizeof(float));
    if (!temp_grid) return;
    
    for (int y = 0; y < g_grid_height; y++) {
        for (int x = 0; x < g_grid_width; x++) {
            float sum = 0.0f;
            float weight_sum = 0.0f;
            
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < g_grid_width && ny >= 0 && ny < g_grid_height) {
                        float distance = sqrt(dx * dx + dy * dy);
                        float weight = exp(-distance * distance / (2.0f * NUTRITION_SMOOTHNESS));
                        
                        sum += g_nutrition_grid[ny * g_grid_width + nx] * weight;
                        weight_sum += weight;
                    }
                }
            }
            
            temp_grid[y * g_grid_width + x] = sum / weight_sum;
        }
    }
    
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_nutrition_grid[i] = temp_grid[i];
    }
    
    free(temp_grid);
}

int nutrition_init(void) {
    g_grid_width = (int)ceil(WORLD_WIDTH / NUTRITION_GRID_SIZE);
    g_grid_height = (int)ceil(WORLD_HEIGHT / NUTRITION_GRID_SIZE);
    
    // Allocate both current and original grids
    g_nutrition_grid = malloc(g_grid_width * g_grid_height * sizeof(float));
    g_original_nutrition = malloc(g_grid_width * g_grid_height * sizeof(float));
    
    if (!g_nutrition_grid || !g_original_nutrition) {
        printf("Failed to allocate nutrition grid\n");
        return 0;
    }
    
    generate_perlin_terrain();
    apply_smoothing();
    
    // Copy original values for regeneration reference - PREVENTS TERRAIN DESTRUCTION
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_original_nutrition[i] = g_nutrition_grid[i];
    }
    
    g_visible = 0;
    
    printf("Nutrition v2 initialized: %dx%d grid with original terrain preservation\n", g_grid_width, g_grid_height);
    return 1;
}

void nutrition_cleanup(void) {
    if (g_nutrition_grid) {
        free(g_nutrition_grid);
        g_nutrition_grid = NULL;
    }
    if (g_original_nutrition) {
        free(g_original_nutrition);
        g_original_nutrition = NULL;
    }
}

void nutrition_set_renderer(SDL_Renderer* renderer) {
    g_renderer = renderer;
}

void nutrition_toggle_visibility(void) {
    g_visible = !g_visible;
    printf("Nutrition layer v2: %s\n", g_visible ? "ON" : "OFF");
}

int nutrition_is_visible(void) {
    return g_visible;
}

static void world_to_nutrition_grid(float world_x, float world_y, int* grid_x, int* grid_y) {
    *grid_x = (int)floor((world_x - WORLD_LEFT) / NUTRITION_GRID_SIZE);
    *grid_y = (int)floor((world_y - WORLD_TOP) / NUTRITION_GRID_SIZE);
}

float nutrition_get_value_at(float world_x, float world_y) {
    int grid_x, grid_y;
    world_to_nutrition_grid(world_x, world_y, &grid_x, &grid_y);
    
    if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
        return 0.5f;
    }
    
    return g_nutrition_grid[grid_y * g_grid_width + grid_x];
}

// EXTREME nutrition depletion - VERY VISIBLE EFFECTS
void nutrition_deplete_at_position(float world_x, float world_y, float depletion_amount, float radius) {
    if (!g_nutrition_grid) return;
    
    int center_x = (int)floor((world_x - WORLD_LEFT) / NUTRITION_GRID_SIZE);
    int center_y = (int)floor((world_y - WORLD_TOP) / NUTRITION_GRID_SIZE);
    
    int grid_radius = (int)ceil(radius / NUTRITION_GRID_SIZE);
    
    for (int dy = -grid_radius; dy <= grid_radius; dy++) {
        for (int dx = -grid_radius; dx <= grid_radius; dx++) {
            int grid_x = center_x + dx;
            int grid_y = center_y + dy;
            
            if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
                continue;
            }
            
            float distance = sqrt(dx * dx + dy * dy) * NUTRITION_GRID_SIZE;
            
            if (distance <= radius) {
                float falloff = 1.0f - (distance / radius);
                falloff = falloff * falloff;
                
                float actual_depletion = depletion_amount * falloff;
                
                int index = grid_y * g_grid_width + grid_x;
                g_nutrition_grid[index] -= actual_depletion;
                
                if (g_nutrition_grid[index] < 0.0f) {
                    g_nutrition_grid[index] = 0.0f;
                }
            }
        }
    }
}

// SMART regeneration - preserves original terrain, prevents fade-out
void nutrition_regenerate(void) {
    if (!g_nutrition_grid || !g_original_nutrition) return;
    
    float regen_rate = 0.0002f; // Very slow regeneration
    
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        float current = g_nutrition_grid[i];
        float original = g_original_nutrition[i];
        
        // Only regenerate towards original value, and only if significantly depleted
        if (current < original * 0.8f) { // Only if depleted by more than 20%
            g_nutrition_grid[i] += regen_rate;
            
            // Don't exceed original value - PREVENTS TERRAIN DESTRUCTION
            if (g_nutrition_grid[i] > original) {
                g_nutrition_grid[i] = original;
            }
        }
    }
}

void nutrition_render(void) {
    if (!g_visible || !g_renderer || !g_nutrition_grid) return;
    
    float world_left, world_top, world_right, world_bottom;
    camera_get_viewport_bounds(&world_left, &world_top, &world_right, &world_bottom);
    
    int start_x = (int)floor((world_left - WORLD_LEFT) / NUTRITION_GRID_SIZE) - 1;
    int end_x = (int)ceil((world_right - WORLD_LEFT) / NUTRITION_GRID_SIZE) + 1;
    int start_y = (int)floor((world_top - WORLD_TOP) / NUTRITION_GRID_SIZE) - 1;
    int end_y = (int)ceil((world_bottom - WORLD_TOP) / NUTRITION_GRID_SIZE) + 1;
    
    if (start_x < 0) start_x = 0;
    if (end_x >= g_grid_width) end_x = g_grid_width - 1;
    if (start_y < 0) start_y = 0;
    if (end_y >= g_grid_height) end_y = g_grid_height - 1;
    
    for (int gy = start_y; gy <= end_y; gy++) {
        for (int gx = start_x; gx <= end_x; gx++) {
            float nutrition_value = g_nutrition_grid[gy * g_grid_width + gx];
            
            // INFERNO COLOR SCHEME - FULL RANGE PRESERVED
            int r, g, b;
            
            if (nutrition_value < 0.2f) {
                // Dark purple to purple (very low nutrition)
                float t = nutrition_value / 0.2f;
                r = (int)(20 + t * 60);   // 20-80
                g = (int)(5 + t * 15);    // 5-20
                b = (int)(40 + t * 80);   // 40-120
            } else if (nutrition_value < 0.4f) {
                // Purple to dark red (low nutrition)
                float t = (nutrition_value - 0.2f) / 0.2f;
                r = (int)(80 + t * 100);  // 80-180
                g = (int)(20 + t * 10);   // 20-30
                b = (int)(120 - t * 80);  // 120-40
            } else if (nutrition_value < 0.6f) {
                // Dark red to bright red (medium-low nutrition)
                float t = (nutrition_value - 0.4f) / 0.2f;
                r = (int)(180 + t * 75);  // 180-255
                g = (int)(30 + t * 50);   // 30-80
                b = (int)(40 - t * 30);   // 40-10
            } else if (nutrition_value < 0.8f) {
                // Bright red to orange (medium-high nutrition)
                float t = (nutrition_value - 0.6f) / 0.2f;
                r = 255;
                g = (int)(80 + t * 120);  // 80-200
                b = (int)(10 + t * 20);   // 10-30
            } else {
                // Orange to bright yellow/white (high nutrition)
                float t = (nutrition_value - 0.8f) / 0.2f;
                r = 255;
                g = (int)(200 + t * 55);  // 200-255
                b = (int)(30 + t * 150);  // 30-180
            }
            
            SDL_SetRenderDrawColor(g_renderer, r, g, b, 160); // High visibility
            
            float world_x = WORLD_LEFT + gx * NUTRITION_GRID_SIZE;
            float world_y = WORLD_TOP + gy * NUTRITION_GRID_SIZE;
            
            int screen_x1, screen_y1, screen_x2, screen_y2;
            camera_world_to_screen(world_x, world_y, &screen_x1, &screen_y1);
            camera_world_to_screen(world_x + NUTRITION_GRID_SIZE, world_y + NUTRITION_GRID_SIZE, 
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