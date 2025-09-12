// File: src/nutrition.c - Enhanced with balanced Perlin noise and blur filter
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "types.h"
#include "nutrition.h"
#include "camera.h"

#define NUTRITION_SMOOTHNESS 4.0f  // Reduced smoothing to preserve fragments
#define BLUR_RADIUS 1             // Smaller blur radius for fine details  
#define BLUR_STRENGTH 0.3f        // Much lighter blur to keep fragmentation

static float* g_nutrition_grid = NULL;
static float* g_original_nutrition = NULL;
static int g_grid_width = 0;
static int g_grid_height = 0;
static int g_visible = 0;
static SDL_Renderer* g_renderer = NULL;

static float g_total_nutrition_added = 0.0f;
static float g_total_nutrition_depleted = 0.0f;

// Enhanced Perlin noise permutation table
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

// Balanced noise generation with fine details and even distribution
static void generate_perlin_terrain(void) {
    unsigned int seed = (unsigned int)time(NULL);
    seed ^= (unsigned int)clock();
    seed ^= (unsigned int)getpid();
    seed += (unsigned int)((uintptr_t)&seed);
    srand(seed);
    
    // Shuffle permutation table for unique terrain
    for (int i = 0; i < 256; i++) {
        int j = rand() % 256;
        int temp = permutation[i];
        permutation[i] = permutation[j];
        permutation[j] = temp;
    }
    
    init_perlin();
    
    // Many independent offsets for each layer
    float offset_x[8], offset_y[8];
    for (int i = 0; i < 8; i++) {
        offset_x[i] = ((float)rand() / RAND_MAX) * 3000.0f;
        offset_y[i] = ((float)rand() / RAND_MAX) * 3000.0f;
    }
    
    for (int y = 0; y < g_grid_height; y++) {
        for (int x = 0; x < g_grid_width; x++) {
            float value = 0.0f;
            
            // Base layer: Medium frequency for overall structure
            float px1 = x + offset_x[0];
            float py1 = y + offset_y[0];
            value += octave_perlin(px1, py1, 4, 0.5f, 0.01f) * 0.7f;
            
            // Detail layer 1: Higher frequency patches
            float px2 = x + offset_x[1];
            float py2 = y + offset_y[1];
            value += octave_perlin(px2, py2, 3, 0.6f, 0.03f) * 0.8f;
            
            // Detail layer 2: Fine granular details
            float px3 = x + offset_x[2];
            float py3 = y + offset_y[2];
            value += octave_perlin(px3, py3, 4, 0.4f, 0.08f) * 0.6f;
            
            // Detail layer 3: Micro features
            float px4 = x + offset_x[3];
            float py4 = y + offset_y[3];
            value += octave_perlin(px4, py4, 3, 0.5f, 0.05f) * 0.75f;
            
            // Cross-pattern layer for more variation
            float px5 = x + offset_x[4];
            float py5 = y + offset_y[4];
            float cross_noise = octave_perlin(px5, py5, 2, 0.7f, 0.1f);
            value += cross_noise * 0.1f;
            
            // Two opposing noise patterns for balance
            float px6 = x + offset_x[5];
            float py6 = y + offset_y[5];
            float pattern1 = octave_perlin(px6, py6, 3, 0.5f, 0.04f);
            
            float px7 = x + offset_x[6] + 1000.0f;  // Different offset
            float py7 = y + offset_y[6] + 1000.0f;
            float pattern2 = octave_perlin(px7, py7, 3, 0.5f, 0.04f);
            
            // Combine patterns to create more balanced distribution
            float balanced_pattern = (pattern1 - pattern2) * 0.5f;
            value += balanced_pattern * 0.2f;
            
            // Very fine noise for texture
            float px8 = x + offset_x[7];
            float py8 = y + offset_y[7];
            value += octave_perlin(px8, py8, 6, 0.3f, 0.25f) * 0.08f;
            
            // Normalize from [-1, 1] to [0, 1]
            value = (value + 1.0f) * 0.5f;
            
            // Add controlled randomness
            float random_factor = ((float)rand() / RAND_MAX - 0.5f) * 0.08f;
            value += random_factor;
            
            // Apply sine wave modulation for more organic feel
            float wave_x = sin(x * 0.02f + offset_x[0] * 0.01f) * 0.03f;
            float wave_y = cos(y * 0.015f + offset_y[0] * 0.01f) * 0.03f;
            value += wave_x + wave_y;
            
            // Clamp to valid range
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            
            g_nutrition_grid[y * g_grid_width + x] = value;
        }
    }
    
    // Balanced post-processing to maintain even distribution
    float min_val = 1.0f, max_val = 0.0f;
    float sum = 0.0f;
    int count = g_grid_width * g_grid_height;
    
    // Calculate statistics
    for (int i = 0; i < count; i++) {
        float val = g_nutrition_grid[i];
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
        sum += val;
    }
    
    float mean = sum / count;
    float range = max_val - min_val;
    
    if (range > 0.0f) {
        // Normalize and apply S-curve for better distribution
        for (int i = 0; i < count; i++) {
            float normalized = (g_nutrition_grid[i] - min_val) / range;
            
            // S-curve function for more balanced hot/cold distribution
            float s_curved = 0.5f + 0.5f * tanh((normalized - 0.5f) * 3.0f);
            
            // Blend original with s-curve
            g_nutrition_grid[i] = normalized * 0.3f + s_curved * 0.7f;
        }
    }
}

// Fine-tuned smoothing for detail preservation
static void apply_smoothing(void) {
    float* temp_grid = malloc(g_grid_width * g_grid_height * sizeof(float));
    if (!temp_grid) return;
    
    for (int y = 0; y < g_grid_height; y++) {
        for (int x = 0; x < g_grid_width; x++) {
            float sum = 0.0f;
            float weight_sum = 0.0f;
            
            // Very light smoothing to preserve fine details
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
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
    
    // Apply minimal smoothing to maintain detail
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_nutrition_grid[i] = g_nutrition_grid[i] * 0.7f + temp_grid[i] * 0.3f;
    }
    
    free(temp_grid);
}

// Additional blur filter for smoothing the mask
static void apply_blur_filter(void) {
    if (BLUR_STRENGTH <= 0.0f) return;
    
    float* temp_grid = malloc(g_grid_width * g_grid_height * sizeof(float));
    if (!temp_grid) return;
    
    // Multi-pass blur for smoother results
    int blur_passes = 2;
    float* source_grid = g_nutrition_grid;
    float* target_grid = temp_grid;
    
    for (int pass = 0; pass < blur_passes; pass++) {
        for (int y = 0; y < g_grid_height; y++) {
            for (int x = 0; x < g_grid_width; x++) {
                float sum = 0.0f;
                float weight_sum = 0.0f;
                
                // Gaussian-like blur kernel
                for (int dy = -BLUR_RADIUS; dy <= BLUR_RADIUS; dy++) {
                    for (int dx = -BLUR_RADIUS; dx <= BLUR_RADIUS; dx++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        if (nx >= 0 && nx < g_grid_width && ny >= 0 && ny < g_grid_height) {
                            float distance = sqrt(dx * dx + dy * dy);
                            // Gaussian weight function
                            float weight = exp(-distance * distance / (2.0f * BLUR_RADIUS * BLUR_RADIUS));
                            
                            sum += source_grid[ny * g_grid_width + nx] * weight;
                            weight_sum += weight;
                        }
                    }
                }
                
                target_grid[y * g_grid_width + x] = sum / weight_sum;
            }
        }
        
        // Swap buffers for next pass
        if (pass < blur_passes - 1) {
            float* swap = source_grid;
            source_grid = target_grid;
            target_grid = swap;
        }
    }
    
    // Blend original with blurred version
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_nutrition_grid[i] = g_nutrition_grid[i] * (1.0f - BLUR_STRENGTH) + 
                              target_grid[i] * BLUR_STRENGTH;
    }
    
    free(temp_grid);
}

int nutrition_init(void) {
    g_grid_width = (int)ceil(WORLD_WIDTH / LAYER_GRID_SIZE);
    g_grid_height = (int)ceil(WORLD_HEIGHT / LAYER_GRID_SIZE);
    
    g_nutrition_grid = malloc(g_grid_width * g_grid_height * sizeof(float));
    g_original_nutrition = malloc(g_grid_width * g_grid_height * sizeof(float));
    
    if (!g_nutrition_grid || !g_original_nutrition) {
        printf("Failed to allocate nutrition grid\n");
        return 0;
    }
    
    generate_perlin_terrain();
    apply_smoothing();
    apply_blur_filter();  // Additional blur pass for smoother mask
    
    // Store original values
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        g_original_nutrition[i] = g_nutrition_grid[i];
    }
    
    g_visible = 0;
    g_total_nutrition_added = 0.0f;
    g_total_nutrition_depleted = 0.0f;
    
    printf("Nutrition layer with blur filter initialized: %dx%d grid (blur radius: %d, strength: %.1f)\n", 
           g_grid_width, g_grid_height, BLUR_RADIUS, BLUR_STRENGTH);
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
    
    printf("Nutrition cycle stats - Added: %.2f, Depleted: %.2f, Balance: %.2f\n",
           g_total_nutrition_added, g_total_nutrition_depleted, 
           g_total_nutrition_added - g_total_nutrition_depleted);
}

void nutrition_set_renderer(SDL_Renderer* renderer) {
    g_renderer = renderer;
}

void nutrition_toggle_visibility(void) {
    g_visible = !g_visible;
    printf("Nutrition layer: %s\n", g_visible ? "ON" : "OFF");
}

int nutrition_is_visible(void) {
    return g_visible;
}

static void world_to_nutrition_grid(float world_x, float world_y, int* grid_x, int* grid_y) {
    *grid_x = (int)floor((world_x - WORLD_LEFT) / LAYER_GRID_SIZE);
    *grid_y = (int)floor((world_y - WORLD_TOP) / LAYER_GRID_SIZE);
}

float nutrition_get_value_at(float world_x, float world_y) {
    int grid_x, grid_y;
    world_to_nutrition_grid(world_x, world_y, &grid_x, &grid_y);
    
    if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
        return 0.5f;
    }
    
    return g_nutrition_grid[grid_y * g_grid_width + grid_x];
}

void nutrition_deplete_at_position(float world_x, float world_y, float depletion_amount, float radius) {
    if (!g_nutrition_grid) return;
    
    int center_x = (int)floor((world_x - WORLD_LEFT) / LAYER_GRID_SIZE);
    int center_y = (int)floor((world_y - WORLD_TOP) / LAYER_GRID_SIZE);
    
    int grid_radius = (int)ceil(radius / LAYER_GRID_SIZE);
    
    float total_depleted = 0.0f;
    
    for (int dy = -grid_radius; dy <= grid_radius; dy++) {
        for (int dx = -grid_radius; dx <= grid_radius; dx++) {
            int grid_x = center_x + dx;
            int grid_y = center_y + dy;
            
            if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
                continue;
            }
            
            float distance = sqrt(dx * dx + dy * dy) * LAYER_GRID_SIZE;
            
            if (distance <= radius) {
                float falloff = 1.0f - (distance / radius);
                falloff = falloff * falloff;
                
                float actual_depletion = depletion_amount * falloff;
                
                int index = grid_y * g_grid_width + grid_x;
                float old_value = g_nutrition_grid[index];
                g_nutrition_grid[index] -= actual_depletion;
                
                if (g_nutrition_grid[index] < 0.0f) {
                    g_nutrition_grid[index] = 0.0f;
                }
                
                total_depleted += (old_value - g_nutrition_grid[index]);
            }
        }
    }
    
    g_total_nutrition_depleted += total_depleted;
}

void nutrition_add_at_position(float world_x, float world_y, float addition_amount, float radius) {
    if (!g_nutrition_grid) return;
    
    int center_x = (int)floor((world_x - WORLD_LEFT) / LAYER_GRID_SIZE);
    int center_y = (int)floor((world_y - WORLD_TOP) / LAYER_GRID_SIZE);
    
    int grid_radius = (int)ceil(radius / LAYER_GRID_SIZE);
    if (grid_radius < 2) grid_radius = 2;
    
    float total_added = 0.0f;
    
    for (int dy = -grid_radius; dy <= grid_radius; dy++) {
        for (int dx = -grid_radius; dx <= grid_radius; dx++) {
            int grid_x = center_x + dx;
            int grid_y = center_y + dy;
            
            if (grid_x < 0 || grid_x >= g_grid_width || grid_y < 0 || grid_y >= g_grid_height) {
                continue;
            }
            
            float distance = sqrt(dx * dx + dy * dy) * LAYER_GRID_SIZE;
            
            if (distance <= radius) {
                float falloff = 1.0f - (distance / radius);
                falloff = falloff * falloff;
                
                float cell_nutrition = addition_amount * falloff;
                
                int index = grid_y * g_grid_width + grid_x;
                float old_value = g_nutrition_grid[index];
                
                g_nutrition_grid[index] += cell_nutrition;
                
                if (g_nutrition_grid[index] > 3.0f) {
                    g_nutrition_grid[index] = 3.0f;
                }
                
                float actually_added = g_nutrition_grid[index] - old_value;
                total_added += actually_added;
            }
        }
    }
    
    g_total_nutrition_added += total_added;
}

void nutrition_regenerate(void) {
    if (!g_nutrition_grid || !g_original_nutrition) return;
    
    float regen_rate = 0.0002f;
    
    for (int i = 0; i < g_grid_width * g_grid_height; i++) {
        float current = g_nutrition_grid[i];
        float original = g_original_nutrition[i];
        
        if (current < original * 0.8f) {
            g_nutrition_grid[i] += regen_rate;
            
            if (g_nutrition_grid[i] > original) {
                g_nutrition_grid[i] = original;
            }
        }
    }
}

static void value_to_nutrition_color(float value, int* r, int* g, int* b) {
    if (value < 0.0f) value = 0.0f;
    if (value > 2.0f) value = 2.0f;
    
    if (value > 1.0f) {
        float excess = value - 1.0f;
        *r = 255;
        *g = 255 - (int)(excess * 127);
        *b = 0;
    } else {
        float h = (1.0f - value) * 5.0f;
        int i = (int)floor(h);
        float f = h - i;
        
        switch (i) {
            case 0:
                *r = 255;
                *g = (int)(255 * f);
                *b = 0;
                break;
            case 1:
                *r = (int)(255 * (1.0f - f));
                *g = 255;
                *b = 0;
                break;
            case 2:
                *r = 0;
                *g = 255;
                *b = (int)(255 * f);
                break;
            case 3:
                *r = 0;
                *g = (int)(255 * (1.0f - f));
                *b = 255;
                break;
            case 4:
                *r = (int)(255 * f);
                *g = 0;
                *b = 255;
                break;
            default:
                *r = 255;
                *g = 0;
                *b = 255;
                break;
        }
    }
}

void nutrition_render(void) {
    if (!g_visible || !g_renderer || !g_nutrition_grid) return;
    
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
            float nutrition_value = g_nutrition_grid[gy * g_grid_width + gx];
            
            int r, g, b;
            value_to_nutrition_color(nutrition_value, &r, &g, &b);
            
            int alpha = 180;
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

float nutrition_get_total_added(void) {
    return g_total_nutrition_added;
}

float nutrition_get_total_depleted(void) {
    return g_total_nutrition_depleted;
}

float nutrition_get_balance(void) {
    return g_total_nutrition_added - g_total_nutrition_depleted;
}