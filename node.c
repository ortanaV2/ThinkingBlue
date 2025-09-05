#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Core constants
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define MAX_NODES 10000000
#define MAX_CHAINS 10000000
#define MAX_PLANT_TYPES 64
#define MAX_NAME_LENGTH 64

// Physics parameters
#define OPTIMAL_DISTANCE 50.0f
#define REPULSION_FORCE 0.05f
#define CHAIN_FORCE 0.05f
#define WATER_DRAG 0.95f

// Rendering parameters
#define NODE_RADIUS 5
#define CHAIN_THICKNESS 6

// Camera parameters
#define CAMERA_SPEED 5.0f
#define ZOOM_SPEED 0.1f
#define MIN_ZOOM 0.1f
#define MAX_ZOOM 5.0f

// World configuration
#define WORLD_WIDTH 15000.0f
#define WORLD_HEIGHT 15000.0f
#define WORLD_CENTER_X 0.0f
#define WORLD_CENTER_Y 0.0f

// Derived world bounds
#define WORLD_LEFT (WORLD_CENTER_X - WORLD_WIDTH / 2.0f)
#define WORLD_RIGHT (WORLD_CENTER_X + WORLD_WIDTH / 2.0f)
#define WORLD_TOP (WORLD_CENTER_Y - WORLD_HEIGHT / 2.0f)
#define WORLD_BOTTOM (WORLD_CENTER_Y + WORLD_HEIGHT / 2.0f)

// Spatial optimization
#define GRID_SIZE 40.0f
#define MAX_NODES_PER_CELL 200

#define M_PI 3.14159265358979323846

// Plant type configuration
typedef struct {
    char name[MAX_NAME_LENGTH];
    float growth_probability;
    int growth_attempts;
    int max_branches;
    float branch_distance;
    
    // Node colors (RGB 0-255)
    int node_r, node_g, node_b;
    
    // Chain colors (RGB 0-255)
    int chain_r, chain_g, chain_b;
    
    int active;
} PlantType;

// Node structure
typedef struct {
    float x, y;       // Position
    float vx, vy;     // Velocity
    int active;       // Active flag
    int can_grow;     // Growth capability flag
    int plant_type;   // Plant type index
    int branch_count; // Current branch count
    int age;          // Age in frames
} Node;

// Chain structure connecting two nodes
typedef struct {
    int node1, node2;
    int active;
    int plant_type;   // Plant type for rendering
    int age;          // Age in frames
    float curve_strength; // Curve strength (-1.0 to 1.0)
    float curve_offset;   // Perpendicular offset for curve direction
} Chain;

// Camera for viewport control
typedef struct {
    float x, y;       // World position
    float zoom;       // Zoom level
} Camera;

// Spatial grid cell for optimization
typedef struct {
    int node_indices[MAX_NODES_PER_CELL];
    int count;
} GridCell;

// Global simulation state
static Node* g_nodes = NULL;
static Chain* g_chains = NULL;
static GridCell* g_grid = NULL;
static PlantType g_plant_types[MAX_PLANT_TYPES];
static int g_plant_type_count = 0;
static int g_current_plant_type = 0;

static int g_grid_width = 0;
static int g_grid_height = 0;

static int g_node_count = 0;
static int g_chain_count = 0;
static int g_selected_node = -1;
static int g_selection_mode = 0;
static Camera g_camera = {0};
static int g_keys[4] = {0};
static int g_frame_counter = 0;

// Forward declarations
static void draw_thick_line(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness);

// Age color calculation - makes colors darker but not fully brown
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

// Draw thick line for basic line rendering
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

// Draw curved line using quadratic bezier curve
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
    int segments = (int)(length / 8.0f) + 3; // More segments for smoother curves
    if (segments > 20) segments = 20; // Cap segments for performance
    
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

// Parse color from hex string (e.g., "#FF0000" or "FF0000")
static void parse_color(const char* color_str, int* r, int* g, int* b) {
    const char* hex = color_str;
    if (hex[0] == '#') hex++; // Skip '#' if present
    
    unsigned int color = (unsigned int)strtoul(hex, NULL, 16);
    *r = (color >> 16) & 0xFF;
    *g = (color >> 8) & 0xFF;
    *b = color & 0xFF;
}

// Load plant configurations from file
static int load_plant_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open config file '%s'\n", filename);
        return 0;
    }
    
    char line[256];
    PlantType* current_plant = NULL;
    g_plant_type_count = 0;
    
    while (fgets(line, sizeof(line), file) && g_plant_type_count < MAX_PLANT_TYPES) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;
        
        // Check for plant type declaration [PlantName]
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            current_plant = &g_plant_types[g_plant_type_count];
            memset(current_plant, 0, sizeof(PlantType));
            
            // Extract plant name
            strncpy(current_plant->name, line + 1, strlen(line) - 2);
            current_plant->name[strlen(line) - 2] = '\0';
            current_plant->active = 1;
            
            // Set default values
            current_plant->growth_probability = 0.02f;
            current_plant->growth_attempts = 5;
            current_plant->max_branches = 3;
            current_plant->branch_distance = OPTIMAL_DISTANCE;
            current_plant->node_r = 150;
            current_plant->node_g = 255;
            current_plant->node_b = 150;
            current_plant->chain_r = 100;
            current_plant->chain_g = 200;
            current_plant->chain_b = 100;
            
            g_plant_type_count++;
            continue;
        }
        
        if (!current_plant) continue;
        
        // Parse key=value pairs
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Trim whitespace
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        // Parse configuration values
        if (strcmp(key, "growth_probability") == 0) {
            current_plant->growth_probability = (float)atof(value);
        } else if (strcmp(key, "growth_attempts") == 0) {
            current_plant->growth_attempts = atoi(value);
        } else if (strcmp(key, "max_branches") == 0) {
            current_plant->max_branches = atoi(value);
        } else if (strcmp(key, "branch_distance") == 0) {
            current_plant->branch_distance = (float)atof(value);
        } else if (strcmp(key, "node_color") == 0) {
            parse_color(value, &current_plant->node_r, &current_plant->node_g, &current_plant->node_b);
        } else if (strcmp(key, "chain_color") == 0) {
            parse_color(value, &current_plant->chain_r, &current_plant->chain_g, &current_plant->chain_b);
        }
    }
    
    fclose(file);
    
    printf("Loaded %d plant types from config\n", g_plant_type_count);
    for (int i = 0; i < g_plant_type_count; i++) {
        PlantType* pt = &g_plant_types[i];
        printf("  %s: prob=%.3f, attempts=%d, branches=%d, distance=%.1f\n",
               pt->name, pt->growth_probability, pt->growth_attempts, 
               pt->max_branches, pt->branch_distance);
    }
    
    return g_plant_type_count > 0;
}

// Grid access with bounds checking
static GridCell* get_grid_cell(int grid_x, int grid_y) {
    if (grid_x < 0 || grid_x >= g_grid_width || 
        grid_y < 0 || grid_y >= g_grid_height) {
        return NULL;
    }
    return &g_grid[grid_y * g_grid_width + grid_x];
}

// Convert world coordinates to grid coordinates
static void world_to_grid(float world_x, float world_y, int* grid_x, int* grid_y) {
    *grid_x = (int)floor((world_x - WORLD_LEFT) / GRID_SIZE);
    *grid_y = (int)floor((world_y - WORLD_TOP) / GRID_SIZE);
}

// Initialize simulation
static void init_simulation(void) {
    // Allocate main arrays
    g_nodes = (Node*)calloc(MAX_NODES, sizeof(Node));
    g_chains = (Chain*)calloc(MAX_CHAINS, sizeof(Chain));
    
    if (!g_nodes || !g_chains) {
        printf("Failed to allocate memory for simulation\n");
        exit(1);
    }
    
    // Calculate and allocate spatial grid
    g_grid_width = (int)ceil(WORLD_WIDTH / GRID_SIZE);
    g_grid_height = (int)ceil(WORLD_HEIGHT / GRID_SIZE);
    g_grid = (GridCell*)calloc(g_grid_width * g_grid_height, sizeof(GridCell));
    
    if (!g_grid) {
        printf("Failed to allocate spatial grid\n");
        exit(1);
    }
    
    // Initialize state
    g_node_count = 0;
    g_chain_count = 0;
    g_selected_node = -1;
    g_selection_mode = 0;
    g_frame_counter = 0;
    g_current_plant_type = 0;
    
    // Initialize camera at center
    g_camera.x = WINDOW_WIDTH / 2.0f;
    g_camera.y = WINDOW_HEIGHT / 2.0f;
    g_camera.zoom = 1.0f;
    
    // Clear key states
    for (int i = 0; i < 4; i++) {
        g_keys[i] = 0;
    }
    
    printf("Simulation initialized: Grid %dx%d, World %.0fx%.0f\n", 
           g_grid_width, g_grid_height, WORLD_WIDTH, WORLD_HEIGHT);
}

// Clean up simulation resources
static void cleanup_simulation(void) {
    if (g_nodes) {
        free(g_nodes);
        g_nodes = NULL;
    }
    if (g_chains) {
        free(g_chains);
        g_chains = NULL;
    }
    if (g_grid) {
        free(g_grid);
        g_grid = NULL;
    }
}

// Clear spatial grid
static void clear_grid(void) {
    if (!g_grid) return;
    
    int total_cells = g_grid_width * g_grid_height;
    for (int i = 0; i < total_cells; i++) {
        g_grid[i].count = 0;
    }
}

// Add node to spatial grid
static void add_node_to_grid(int node_index) {
    if (node_index < 0 || node_index >= g_node_count) return;
    if (!g_nodes[node_index].active) return;
    
    int grid_x, grid_y;
    world_to_grid(g_nodes[node_index].x, g_nodes[node_index].y, &grid_x, &grid_y);
    
    GridCell* cell = get_grid_cell(grid_x, grid_y);
    if (cell && cell->count < MAX_NODES_PER_CELL) {
        cell->node_indices[cell->count++] = node_index;
    }
}

// Rebuild spatial grid from current node positions
static void rebuild_grid(void) {
    clear_grid();
    for (int i = 0; i < g_node_count; i++) {
        if (g_nodes[i].active) {
            add_node_to_grid(i);
        }
    }
}

// Convert screen coordinates to world coordinates
static void screen_to_world(int screen_x, int screen_y, float* world_x, float* world_y) {
    *world_x = (screen_x - WINDOW_WIDTH / 2.0f) / g_camera.zoom + g_camera.x;
    *world_y = (screen_y - WINDOW_HEIGHT / 2.0f) / g_camera.zoom + g_camera.y;
}

// Convert world coordinates to screen coordinates
static void world_to_screen(float world_x, float world_y, int* screen_x, int* screen_y) {
    *screen_x = (int)((world_x - g_camera.x) * g_camera.zoom + WINDOW_WIDTH / 2.0f);
    *screen_y = (int)((world_y - g_camera.y) * g_camera.zoom + WINDOW_HEIGHT / 2.0f);
}

// Update camera position based on key input
static void update_camera(void) {
    float move_speed = CAMERA_SPEED / g_camera.zoom;
    
    if (g_keys[0]) g_camera.y -= move_speed; // W
    if (g_keys[1]) g_camera.x -= move_speed; // A
    if (g_keys[2]) g_camera.y += move_speed; // S
    if (g_keys[3]) g_camera.x += move_speed; // D
}

// Zoom camera at mouse position
static void zoom_camera(float zoom_delta, int mouse_x, int mouse_y) {
    // Get world position before zoom
    float world_x_before, world_y_before;
    screen_to_world(mouse_x, mouse_y, &world_x_before, &world_y_before);
    
    // Apply zoom with limits
    g_camera.zoom *= (1.0f + zoom_delta);
    if (g_camera.zoom < MIN_ZOOM) g_camera.zoom = MIN_ZOOM;
    if (g_camera.zoom > MAX_ZOOM) g_camera.zoom = MAX_ZOOM;
    
    // Adjust camera to keep world position under mouse cursor
    float world_x_after, world_y_after;
    screen_to_world(mouse_x, mouse_y, &world_x_after, &world_y_after);
    g_camera.x += world_x_before - world_x_after;
    g_camera.y += world_y_before - world_y_after;
}

// Create new node at position with specific plant type
static int add_node(float x, float y, int plant_type) {
    if (g_node_count >= MAX_NODES) {
        printf("Maximum nodes reached\n");
        return -1;
    }
    
    if (plant_type < 0 || plant_type >= g_plant_type_count) {
        printf("Invalid plant type: %d\n", plant_type);
        return -1;
    }
    
    // Clamp to world bounds
    if (x < WORLD_LEFT) x = WORLD_LEFT;
    if (x > WORLD_RIGHT) x = WORLD_RIGHT;
    if (y < WORLD_TOP) y = WORLD_TOP;
    if (y > WORLD_BOTTOM) y = WORLD_BOTTOM;
    
    Node* node = &g_nodes[g_node_count];
    node->x = x;
    node->y = y;
    node->vx = 0;
    node->vy = 0;
    node->active = 1;
    node->can_grow = 1;
    node->plant_type = plant_type;
    node->branch_count = 0;
    node->age = 0;
    
    return g_node_count++;
}

// Create chain between two nodes
static int add_chain(int node1, int node2) {
    if (g_chain_count >= MAX_CHAINS) {
        printf("Maximum chains reached\n");
        return -1;
    }
    if (node1 == node2) return -1;
    if (node1 < 0 || node1 >= g_node_count) return -1;
    if (node2 < 0 || node2 >= g_node_count) return -1;
    
    // Check for duplicate chains in recent history
    int check_count = (g_chain_count > 1000) ? 1000 : g_chain_count;
    for (int i = g_chain_count - check_count; i < g_chain_count; i++) {
        if (i < 0) continue;
        if (!g_chains[i].active) continue;
        if ((g_chains[i].node1 == node1 && g_chains[i].node2 == node2) ||
            (g_chains[i].node1 == node2 && g_chains[i].node2 == node1)) {
            return -1;
        }
    }
    
    Chain* chain = &g_chains[g_chain_count];
    chain->node1 = node1;
    chain->node2 = node2;
    chain->active = 1;
    chain->plant_type = g_nodes[node1].plant_type; // Use plant type from first node
    chain->age = 0;
    
    // Generate random curve parameters
    chain->curve_strength = ((float)rand() / RAND_MAX - 0.5f) * 0.6f; // -0.3 to 0.3
    chain->curve_offset = ((float)rand() / RAND_MAX - 0.5f) * 20.0f;   // -20 to 20
    
    return g_chain_count++;
}

// Check if position is free of nearby nodes
static int is_position_free(float x, float y, float min_distance) {
    int grid_x, grid_y;
    world_to_grid(x, y, &grid_x, &grid_y);
    
    float min_dist_sq = min_distance * min_distance;
    int search_radius = (int)(min_distance / GRID_SIZE) + 1;
    
    for (int gx = grid_x - search_radius; gx <= grid_x + search_radius; gx++) {
        for (int gy = grid_y - search_radius; gy <= grid_y + search_radius; gy++) {
            GridCell* cell = get_grid_cell(gx, gy);
            if (!cell) continue;
            
            for (int k = 0; k < cell->count; k++) {
                int i = cell->node_indices[k];
                if (i < 0 || i >= g_node_count) continue;
                if (!g_nodes[i].active) continue;
                
                float dx = g_nodes[i].x - x;
                float dy = g_nodes[i].y - y;
                float distance_sq = dx * dx + dy * dy;
                
                if (distance_sq < min_dist_sq) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

// Plant growth system with configurable parameters
static void grow_plants(void) {
    // Run every frame for smoother growth
    int current_node_count = g_node_count;

    int growth_limit = (current_node_count / 100) + 3;  // Scale with node count
    
    int grown = 0;
    
    for (int i = 0; i < current_node_count && grown < growth_limit; i++) {
        if (!g_nodes[i].active || !g_nodes[i].can_grow) continue;
        
        int plant_type = g_nodes[i].plant_type;
        if (plant_type < 0 || plant_type >= g_plant_type_count) continue;
        
        PlantType* pt = &g_plant_types[plant_type];
        
        // Check branch limit and age limit
        if (g_nodes[i].branch_count >= pt->max_branches) continue;
        
        // Stop growing after 30 seconds (1800 frames at 60fps)
        if (g_nodes[i].age > 1800) continue;
        
        if ((float)rand() / RAND_MAX < pt->growth_probability) {
            for (int attempt = 0; attempt < pt->growth_attempts; attempt++) {
                float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
                float new_x = g_nodes[i].x + cos(angle) * pt->branch_distance;
                float new_y = g_nodes[i].y + sin(angle) * pt->branch_distance;
                
                // Check world bounds
                if (new_x < WORLD_LEFT || new_x > WORLD_RIGHT ||
                    new_y < WORLD_TOP || new_y > WORLD_BOTTOM) {
                    continue;
                }
                
                if (is_position_free(new_x, new_y, pt->branch_distance * 0.8f)) {
                    int new_node = add_node(new_x, new_y, plant_type);
                    if (new_node >= 0) {
                        add_chain(i, new_node);
                        g_nodes[i].branch_count++;
                        grown++;
                        break;
                    }
                }
            }
        }
    }
}

// Apply repulsion forces between nearby nodes
static void apply_repulsion_forces(void) {
    float optimal_sq = OPTIMAL_DISTANCE * OPTIMAL_DISTANCE;
    
    for (int gx = 0; gx < g_grid_width; gx++) {
        for (int gy = 0; gy < g_grid_height; gy++) {
            GridCell* cell = get_grid_cell(gx, gy);
            if (!cell) continue;
            
            // Check pairs within same cell
            for (int a = 0; a < cell->count; a++) {
                for (int b = a + 1; b < cell->count; b++) {
                    int i = cell->node_indices[a];
                    int j = cell->node_indices[b];
                    
                    if (i < 0 || i >= g_node_count || j < 0 || j >= g_node_count) continue;
                    if (!g_nodes[i].active || !g_nodes[j].active) continue;
                    
                    float dx = g_nodes[j].x - g_nodes[i].x;
                    float dy = g_nodes[j].y - g_nodes[i].y;
                    float distance_sq = dx * dx + dy * dy;
                    
                    if (distance_sq < optimal_sq && distance_sq > 0) {
                        float distance = sqrt(distance_sq);
                        float force_magnitude = REPULSION_FORCE * (OPTIMAL_DISTANCE - distance) / distance;
                        float fx = -dx * force_magnitude;
                        float fy = -dy * force_magnitude;
                        
                        g_nodes[i].vx += fx;
                        g_nodes[i].vy += fy;
                        g_nodes[j].vx -= fx;
                        g_nodes[j].vy -= fy;
                    }
                }
            }
            
            // Check pairs with adjacent cells
            for (int dgx = -1; dgx <= 1; dgx++) {
                for (int dgy = -1; dgy <= 1; dgy++) {
                    if (dgx == 0 && dgy == 0) continue;
                    if (dgx < 0 || (dgx == 0 && dgy < 0)) continue;
                    
                    GridCell* adj_cell = get_grid_cell(gx + dgx, gy + dgy);
                    if (!adj_cell) continue;
                    
                    for (int a = 0; a < cell->count; a++) {
                        for (int b = 0; b < adj_cell->count; b++) {
                            int i = cell->node_indices[a];
                            int j = adj_cell->node_indices[b];
                            
                            if (i < 0 || i >= g_node_count || j < 0 || j >= g_node_count) continue;
                            if (!g_nodes[i].active || !g_nodes[j].active) continue;
                            
                            float dx = g_nodes[j].x - g_nodes[i].x;
                            float dy = g_nodes[j].y - g_nodes[i].y;
                            float distance_sq = dx * dx + dy * dy;
                            
                            if (distance_sq < optimal_sq && distance_sq > 0) {
                                float distance = sqrt(distance_sq);
                                float force_magnitude = REPULSION_FORCE * (OPTIMAL_DISTANCE - distance) / distance;
                                float fx = -dx * force_magnitude;
                                float fy = -dy * force_magnitude;
                                
                                g_nodes[i].vx += fx;
                                g_nodes[i].vy += fy;
                                g_nodes[j].vx -= fx;
                                g_nodes[j].vy -= fy;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Apply spring forces along chains
static void apply_chain_forces(void) {
    for (int i = 0; i < g_chain_count; i++) {
        if (!g_chains[i].active) continue;
        
        int n1 = g_chains[i].node1;
        int n2 = g_chains[i].node2;
        
        if (n1 < 0 || n1 >= g_node_count || n2 < 0 || n2 >= g_node_count) continue;
        if (!g_nodes[n1].active || !g_nodes[n2].active) continue;
        
        float dx = g_nodes[n2].x - g_nodes[n1].x;
        float dy = g_nodes[n2].y - g_nodes[n1].y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > 0) {
            float force_magnitude = CHAIN_FORCE * (distance - OPTIMAL_DISTANCE) / distance;
            float fx = dx * force_magnitude;
            float fy = dy * force_magnitude;
            
            g_nodes[n1].vx += fx;
            g_nodes[n1].vy += fy;
            g_nodes[n2].vx -= fx;
            g_nodes[n2].vy -= fy;
        }
    }
}

// Update physics simulation
static void update_physics(void) {
    g_frame_counter++;
    
    // Rebuild spatial grid periodically
    if (g_frame_counter % 5 == 0) {
        rebuild_grid();
    }
    
    // Apply forces
    apply_repulsion_forces();
    apply_chain_forces();
    
    // Plant growth - run every frame now
    grow_plants();
    
    // Update positions with drag and world bounds
    for (int i = 0; i < g_node_count; i++) {
        if (!g_nodes[i].active) continue;
        
        // Age nodes
        g_nodes[i].age++;
        
        // Apply drag
        g_nodes[i].vx *= WATER_DRAG;
        g_nodes[i].vy *= WATER_DRAG;
        
        // Update position
        g_nodes[i].x += g_nodes[i].vx;
        g_nodes[i].y += g_nodes[i].vy;
        
        // World bounds collision
        if (g_nodes[i].x < WORLD_LEFT) {
            g_nodes[i].x = WORLD_LEFT;
            g_nodes[i].vx = 0;
        }
        if (g_nodes[i].x > WORLD_RIGHT) {
            g_nodes[i].x = WORLD_RIGHT;
            g_nodes[i].vx = 0;
        }
        if (g_nodes[i].y < WORLD_TOP) {
            g_nodes[i].y = WORLD_TOP;
            g_nodes[i].vy = 0;
        }
        if (g_nodes[i].y > WORLD_BOTTOM) {
            g_nodes[i].y = WORLD_BOTTOM;
            g_nodes[i].vy = 0;
        }
    }
    
    // Age chains
    for (int i = 0; i < g_chain_count; i++) {
        if (g_chains[i].active) {
            g_chains[i].age++;
        }
    }
}

// Main rendering function
static void render(SDL_Renderer* renderer) {
    // Clear background
    SDL_SetRenderDrawColor(renderer, 30, 60, 120, 255);
    SDL_RenderClear(renderer);
    
    // Calculate viewport bounds for culling
    float world_left, world_top, world_right, world_bottom;
    screen_to_world(0, 0, &world_left, &world_top);
    screen_to_world(WINDOW_WIDTH, WINDOW_HEIGHT, &world_right, &world_bottom);
    
    // Render chains
    for (int i = 0; i < g_chain_count; i++) {
        if (!g_chains[i].active) continue;
        
        int n1 = g_chains[i].node1;
        int n2 = g_chains[i].node2;
        
        if (n1 < 0 || n1 >= g_node_count || n2 < 0 || n2 >= g_node_count) continue;
        if (!g_nodes[n1].active || !g_nodes[n2].active) continue;
        
        // Frustum culling
        float min_x = fminf(g_nodes[n1].x, g_nodes[n2].x);
        float max_x = fmaxf(g_nodes[n1].x, g_nodes[n2].x);
        float min_y = fminf(g_nodes[n1].y, g_nodes[n2].y);
        float max_y = fmaxf(g_nodes[n1].y, g_nodes[n2].y);
        
        if (max_x < world_left || min_x > world_right || 
            max_y < world_top || min_y > world_bottom) {
            continue;
        }
        
        // Set chain color based on plant type and age
        int plant_type = g_chains[i].plant_type;
        if (plant_type >= 0 && plant_type < g_plant_type_count) {
            PlantType* pt = &g_plant_types[plant_type];
            int aged_r, aged_g, aged_b;
            calculate_aged_color(pt->chain_r, pt->chain_g, pt->chain_b, g_chains[i].age, &aged_r, &aged_g, &aged_b);
            SDL_SetRenderDrawColor(renderer, aged_r, aged_g, aged_b, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        }
        
        int screen_x1, screen_y1, screen_x2, screen_y2;
        world_to_screen(g_nodes[n1].x, g_nodes[n1].y, &screen_x1, &screen_y1);
        world_to_screen(g_nodes[n2].x, g_nodes[n2].y, &screen_x2, &screen_y2);
        
        int thickness = (int)(CHAIN_THICKNESS * g_camera.zoom);
        if (thickness < 2) thickness = 2; // Minimum thickness of 2
        
        // Use curved line instead of straight line
        draw_curved_line(renderer, screen_x1, screen_y1, screen_x2, screen_y2, 
                        g_chains[i].curve_strength, g_chains[i].curve_offset, thickness);
    }
    
    // Render nodes
    for (int i = 0; i < g_node_count; i++) {
        if (!g_nodes[i].active) continue;
        
        // Frustum culling
        if (g_nodes[i].x < world_left - NODE_RADIUS || g_nodes[i].x > world_right + NODE_RADIUS ||
            g_nodes[i].y < world_top - NODE_RADIUS || g_nodes[i].y > world_bottom + NODE_RADIUS) {
            continue;
        }
        
        int screen_x, screen_y;
        world_to_screen(g_nodes[i].x, g_nodes[i].y, &screen_x, &screen_y);
        
        int scaled_radius = (int)(NODE_RADIUS * g_camera.zoom);
        if (scaled_radius < 1) scaled_radius = 1;
        
        // Set color based on selection state, plant type and age
        if (i == g_selected_node && g_selection_mode == 1) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        } else {
            int plant_type = g_nodes[i].plant_type;
            if (plant_type >= 0 && plant_type < g_plant_type_count) {
                PlantType* pt = &g_plant_types[plant_type];
                int aged_r, aged_g, aged_b;
                calculate_aged_color(pt->node_r, pt->node_g, pt->node_b, g_nodes[i].age, &aged_r, &aged_g, &aged_b);
                SDL_SetRenderDrawColor(renderer, aged_r, aged_g, aged_b, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 150, 255, 150, 255);
            }
        }
        
        // Draw node as circle
        if (scaled_radius <= 2) {
            SDL_RenderDrawPoint(renderer, screen_x, screen_y);
            if (scaled_radius > 1) {
                SDL_RenderDrawPoint(renderer, screen_x-1, screen_y);
                SDL_RenderDrawPoint(renderer, screen_x+1, screen_y);
                SDL_RenderDrawPoint(renderer, screen_x, screen_y-1);
                SDL_RenderDrawPoint(renderer, screen_x, screen_y+1);
            }
        } else {
            for (int dx = -scaled_radius; dx <= scaled_radius; dx++) {
                int dy_max = (int)sqrt(scaled_radius * scaled_radius - dx * dx);
                for (int dy = -dy_max; dy <= dy_max; dy++) {
                    int px = screen_x + dx;
                    int py = screen_y + dy;
                    if (px >= 0 && px < WINDOW_WIDTH && py >= 0 && py < WINDOW_HEIGHT) {
                        SDL_RenderDrawPoint(renderer, px, py);
                    }
                }
            }
        }
    }
    
    SDL_RenderPresent(renderer);
}

// Find node at world position for interaction
static int find_node_at_position(float world_x, float world_y) {
    int grid_x, grid_y;
    world_to_grid(world_x, world_y, &grid_x, &grid_y);
    
    // Check 3x3 grid area around position
    for (int gx = grid_x - 1; gx <= grid_x + 1; gx++) {
        for (int gy = grid_y - 1; gy <= grid_y + 1; gy++) {
            GridCell* cell = get_grid_cell(gx, gy);
            if (!cell) continue;
            
            for (int k = 0; k < cell->count; k++) {
                int i = cell->node_indices[k];
                if (i < 0 || i >= g_node_count) continue;
                if (!g_nodes[i].active) continue;
                
                float dx = g_nodes[i].x - world_x;
                float dy = g_nodes[i].y - world_y;
                float distance = dx * dx + dy * dy;
                
                float threshold = NODE_RADIUS * 2 / g_camera.zoom;
                if (distance <= threshold * threshold) {
                    return i;
                }
            }
        }
    }
    return -1;
}

// Handle mouse click events
static void handle_mouse_click(int screen_x, int screen_y, int button) {
    float world_x, world_y;
    screen_to_world(screen_x, screen_y, &world_x, &world_y);
    
    if (button == SDL_BUTTON_LEFT) {
        // Create new node with current plant type
        if (g_plant_type_count > 0) {
            int new_node = add_node(world_x, world_y, g_current_plant_type);
            if (new_node >= 0) {
                printf("Created %s node %d at (%.1f, %.1f)\n", 
                       g_plant_types[g_current_plant_type].name, new_node, world_x, world_y);
            }
        }
    } else if (button == SDL_BUTTON_RIGHT) {
        // Node selection for chaining
        int clicked_node = find_node_at_position(world_x, world_y);
        
        if (clicked_node >= 0) {
            if (g_selection_mode == 0) {
                g_selected_node = clicked_node;
                g_selection_mode = 1;
                printf("Selected node %d for chaining\n", clicked_node);
            } else if (g_selection_mode == 1) {
                if (clicked_node != g_selected_node) {
                    int chain_id = add_chain(g_selected_node, clicked_node);
                    if (chain_id >= 0) {
                        printf("Created chain between nodes %d and %d\n", g_selected_node, clicked_node);
                    }
                }
                g_selection_mode = 0;
                g_selected_node = -1;
            }
        } else {
            g_selection_mode = 0;
            g_selected_node = -1;
        }
    }
}

// Main program entry point
int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));
    
    // Load plant configuration
    if (!load_plant_config("items.conf")) {
        printf("Error: No plant types loaded. Please create items.conf file.\n");
        return 1;
    }
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow("Customizable Plant Ecosystem",
                                         SDL_WINDOWPOS_CENTERED,
                                         SDL_WINDOWPOS_CENTERED,
                                         WINDOW_WIDTH, WINDOW_HEIGHT,
                                         SDL_WINDOW_SHOWN);
    
    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    init_simulation();
    
    int running = 1;
    SDL_Event event;
    int mouse_x = 0, mouse_y = 0;
    
    printf("\nControls:\n");
    printf("  Left click: Create node (current: %s)\n", 
           g_plant_type_count > 0 ? g_plant_types[g_current_plant_type].name : "none");
    printf("  Right click: Select nodes for chaining\n");
    printf("  WASD: Move camera\n");
    printf("  Mouse wheel: Zoom in/out\n");
    printf("  1-9: Switch plant type\n");
    printf("  ESC: Exit\n\n");
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    else if (event.key.keysym.sym == SDLK_w) g_keys[0] = 1;
                    else if (event.key.keysym.sym == SDLK_a) g_keys[1] = 1;
                    else if (event.key.keysym.sym == SDLK_s) g_keys[2] = 1;
                    else if (event.key.keysym.sym == SDLK_d) g_keys[3] = 1;
                    // Plant type selection
                    else if (event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_9) {
                        int plant_index = event.key.keysym.sym - SDLK_1;
                        if (plant_index < g_plant_type_count) {
                            g_current_plant_type = plant_index;
                            printf("Selected plant type: %s\n", g_plant_types[g_current_plant_type].name);
                        }
                    }
                    break;
                    
                case SDL_KEYUP:
                    if (event.key.keysym.sym == SDLK_w) g_keys[0] = 0;
                    else if (event.key.keysym.sym == SDLK_a) g_keys[1] = 0;
                    else if (event.key.keysym.sym == SDLK_s) g_keys[2] = 0;
                    else if (event.key.keysym.sym == SDLK_d) g_keys[3] = 0;
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    handle_mouse_click(event.button.x, event.button.y, event.button.button);
                    break;
                    
                case SDL_MOUSEWHEEL:
                    SDL_GetMouseState(&mouse_x, &mouse_y);
                    if (event.wheel.y > 0) {
                        zoom_camera(ZOOM_SPEED, mouse_x, mouse_y);
                    } else if (event.wheel.y < 0) {
                        zoom_camera(-ZOOM_SPEED, mouse_x, mouse_y);
                    }
                    break;
                    
                case SDL_MOUSEMOTION:
                    mouse_x = event.motion.x;
                    mouse_y = event.motion.y;
                    break;
            }
        }
        
        update_camera();
        update_physics();
        render(renderer);
        
        SDL_Delay(16);
    }
    
    cleanup_simulation();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}