#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <SDL2/SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

// === CONFIGURATION ===
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PIXEL_SIZE 3
#define GRID_WIDTH (WINDOW_WIDTH / PIXEL_SIZE)
#define GRID_HEIGHT (WINDOW_HEIGHT / PIXEL_SIZE)
#define MAX_SPECIES 10
#define MAX_NAME_LEN 32
#define CONFIG_FILE "plants.yml"
#define AGING_DIVISOR 100        // Plants age 100x slower (1 age per 100 frames)

// Softbody dynamics configuration
#define MAX_DISPLACEMENT 3       // Maximum distance a plant can be pushed
#define PUSH_STRENGTH 0.8f       // How strong the push effect is
#define SPRING_BACK 0.1f         // How quickly plants return to original position

// === CELL STATES ===
typedef enum {
    CELL_EMPTY = 0,
    CELL_PLANT = 1,
    CELL_DEAD_PLANT = 2,
    CELL_NUTRIENT = 3
} CellState;

// === STRUCTURES ===
typedef struct {
    int r, g, b;
} Color;

typedef struct {
    char name[MAX_NAME_LEN];
    float growth_rate;
    int min_neighbors;
    int max_neighbors;
    int max_crowding;
    int death_age;
    int max_energy;
    int base_metabolism;
    int nutrient_absorption_radius;
    Color base_color;
    int energy_brightness;
    int generation_red_boost;
    int age_blue_factor;
    int crowding_stress;
    int decompose_energy;
    int decompose_time;
    
    // New branching properties
    float branching_factor;          // 0.0-1.0: Probability of branching growth
    int branching_distance;          // How far branches can extend
    int branching_angle_variance;    // 0-8: Directional spread of branches
    
    // New bloom properties  
    float bloom_probability;         // 0.0-1.0: Chance of blooming per frame
    int bloom_duration;              // How long blooms last
    Color bloom_color;               // Color of bloom pigments
    int bloom_intensity;             // Brightness of bloom effect
    int bloom_min_age;               // Minimum age before blooming
} Species;

typedef struct {
    CellState state;
    int age;                // Actual age in "life units" 
    int age_frames;         // Frame counter for aging
    int energy;
    int generation;
    int species_id;
    int bloom_timer;        // Countdown for bloom duration
    int bloom_frames;       // Frame counter for bloom timing
    int branch_direction;   // Preferred growth direction for branching
    int is_bloom_center;    // 1 if this cell is center of a bloom
    int bloom_id;           // Unique ID for this bloom cluster
    
    // Grid-based softbody dynamics
    int original_x;         // Original grid position X
    int original_y;         // Original grid position Y
    int displaced_x;        // Current displaced grid position X
    int displaced_y;        // Current displaced grid position Y
    int is_displaced;       // 1 if currently displaced from original position
    int push_strength;      // How strongly this cell is being pushed (for visual feedback)
} Cell;

typedef struct {
    Cell* grid;
    Cell* next_grid;
    Species* species_list;
    int species_count;
    int current_species;
    SDL_Renderer* renderer;
    int running;
    int frame_count;
    int mouse_pressed;
    
    // Softbody interaction system
    int cursor_x, cursor_y;         // Current cursor position in grid coordinates
    int softbody_mode;              // 1 when Ctrl+Left is held
    float cursor_radius;            // Radius of cursor influence
} Simulation;

// === FORWARD DECLARATIONS ===
void setup_console(void);
int load_species_config(Simulation* sim, const char* filename);
void cleanup_simulation(Simulation* sim);
int init_simulation(Simulation* sim);
void reset_simulation(Simulation* sim);
Cell* get_cell(Simulation* sim, int x, int y);
int get_grid_index(int x, int y);
void plant_seed(Simulation* sim, int x, int y, int species_id);
void spawn_nutrients(Simulation* sim);
void setup_initial_ecosystem(Simulation* sim);
void create_bloom_cluster(Simulation* sim, int center_x, int center_y, int species_id);
int count_neighbors_by_state(Simulation* sim, int x, int y, CellState state);
void count_neighbors_by_species(Simulation* sim, int x, int y, int* species_counts);
int absorb_nutrients(Simulation* sim, int x, int y, int radius);
void update_empty_cell(Simulation* sim, int x, int y, Cell* current, Cell* next);
void update_plant_cell(Simulation* sim, int x, int y, Cell* current, Cell* next);
void update_dead_plant_cell(Simulation* sim, int x, int y, Cell* current, Cell* next);
void update_nutrient_cell(Simulation* sim, int x, int y, Cell* current, Cell* next);
void update_simulation(Simulation* sim);
Color calculate_plant_color(Simulation* sim, Cell* cell);
Color calculate_dead_plant_color(Simulation* sim, Cell* cell);
Color calculate_nutrient_color(Cell* cell);
void render_simulation(Simulation* sim);
void handle_events(Simulation* sim);
void print_species_info(Simulation* sim);
void print_controls(Simulation* sim);

// Global bloom counter for unique IDs
static int global_bloom_id = 1;

// === UTILITY FUNCTIONS ===
void setup_console(void) {
#ifdef _WIN32
    AllocConsole();
    FILE* temp;
    freopen_s(&temp, "CONOUT$", "w", stdout);
    freopen_s(&temp, "CONIN$", "r", stdin);
    SetConsoleTitle("Plant Simulation Console");
#endif
}

void trim_string(char* str) {
    char* start = str;
    char* end = str + strlen(str) - 1;
    
    while (*start == ' ' || *start == '\t') start++;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

void parse_yaml_line(char* line, char* key, char* value) {
    char* colon = strchr(line, ':');
    if (!colon) {
        key[0] = '\0';
        value[0] = '\0';
        return;
    }
    
    *colon = '\0';
    strcpy(key, line);
    strcpy(value, colon + 1);
    trim_string(key);
    trim_string(value);
}

// === GRID FUNCTIONS ===
int get_grid_index(int x, int y) {
    return y * GRID_WIDTH + x;
}

Cell* get_cell(Simulation* sim, int x, int y) {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return NULL;
    return &sim->grid[get_grid_index(x, y)];
}

// === CONFIGURATION LOADING ===
int load_species_config(Simulation* sim, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not load %s\n", filename);
        return 0;
    }
    
    printf("Loading species configuration from %s...\n", filename);
    
    // Allocate memory for species
    sim->species_list = malloc(MAX_SPECIES * sizeof(Species));
    if (!sim->species_list) {
        printf("Error: Failed to allocate memory for species\n");
        fclose(file);
        return 0;
    }
    
    char line[256];
    Species* current_species = NULL;
    int in_base_color = 0;
    sim->species_count = 0;
    
    while (fgets(line, sizeof(line), file) && sim->species_count < MAX_SPECIES) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        // New species definition
        if (line[0] != ' ' && line[0] != '\t' && strchr(line, ':')) {
            char* colon = strchr(line, ':');
            *colon = '\0';
            trim_string(line);
            
            current_species = &sim->species_list[sim->species_count];
            strncpy(current_species->name, line, MAX_NAME_LEN - 1);
            current_species->name[MAX_NAME_LEN - 1] = '\0';
            
            // Set defaults
            current_species->growth_rate = 0.3f;
            current_species->min_neighbors = 2;
            current_species->max_neighbors = 3;
            current_species->max_crowding = 5;
            current_species->death_age = 80;
            current_species->max_energy = 100;
            current_species->base_metabolism = 2;
            current_species->nutrient_absorption_radius = 2;
            current_species->base_color = (Color){50, 150, 50};
            current_species->energy_brightness = 80;
            current_species->generation_red_boost = 10;
            current_species->age_blue_factor = 5;
            current_species->crowding_stress = 5;
            current_species->decompose_energy = 25;
            current_species->decompose_time = 20;
            
            // New branching defaults
            current_species->branching_factor = 0.0f;
            current_species->branching_distance = 1;
            current_species->branching_angle_variance = 2;
            
            // New bloom defaults
            current_species->bloom_probability = 0.0f;
            current_species->bloom_duration = 10;
            current_species->bloom_color = (Color){255, 100, 150};
            current_species->bloom_intensity = 100;
            current_species->bloom_min_age = 20;
            
            sim->species_count++;
            continue;
        }
        
        // Parse species properties
        if (current_species && (line[0] == ' ' || line[0] == '\t')) {
            char key[64], value[64];
            parse_yaml_line(line, key, value);
            
            if (strlen(key) == 0) continue;
            
            // Handle nested base_color
            if (strcmp(key, "base_color") == 0) {
                in_base_color = 1;
            } else if (in_base_color) {
                if (strcmp(key, "r") == 0) current_species->base_color.r = atoi(value);
                else if (strcmp(key, "g") == 0) current_species->base_color.g = atoi(value);
                else if (strcmp(key, "b") == 0) {
                    current_species->base_color.b = atoi(value);
                    in_base_color = 0;
                }
            } else if (strcmp(key, "bloom_color") == 0) {
                // Handle bloom_color separately
                continue; // Will be handled in next iterations
            } else {
                // Parse all properties dynamically
                if (strcmp(key, "growth_rate") == 0) current_species->growth_rate = atof(value);
                else if (strcmp(key, "min_neighbors") == 0) current_species->min_neighbors = atoi(value);
                else if (strcmp(key, "max_neighbors") == 0) current_species->max_neighbors = atoi(value);
                else if (strcmp(key, "max_crowding") == 0) current_species->max_crowding = atoi(value);
                else if (strcmp(key, "death_age") == 0) current_species->death_age = atoi(value);
                else if (strcmp(key, "max_energy") == 0) current_species->max_energy = (int)atof(value);
                else if (strcmp(key, "base_metabolism") == 0) current_species->base_metabolism = (int)atof(value);
                else if (strcmp(key, "nutrient_absorption_radius") == 0) current_species->nutrient_absorption_radius = atoi(value);
                else if (strcmp(key, "energy_brightness") == 0) current_species->energy_brightness = atoi(value);
                else if (strcmp(key, "generation_red_boost") == 0) current_species->generation_red_boost = atoi(value);
                else if (strcmp(key, "age_blue_factor") == 0) current_species->age_blue_factor = atoi(value);
                else if (strcmp(key, "crowding_stress") == 0) current_species->crowding_stress = (int)atof(value);
                else if (strcmp(key, "decompose_energy") == 0) current_species->decompose_energy = (int)atof(value);
                else if (strcmp(key, "decompose_time") == 0) current_species->decompose_time = atoi(value);
                
                // New branching properties
                else if (strcmp(key, "branching_factor") == 0) current_species->branching_factor = atof(value);
                else if (strcmp(key, "branching_distance") == 0) current_species->branching_distance = atoi(value);
                else if (strcmp(key, "branching_angle_variance") == 0) current_species->branching_angle_variance = atoi(value);
                
                // New bloom properties
                else if (strcmp(key, "bloom_probability") == 0) current_species->bloom_probability = atof(value);
                else if (strcmp(key, "bloom_duration") == 0) current_species->bloom_duration = atoi(value);
                else if (strcmp(key, "bloom_intensity") == 0) current_species->bloom_intensity = atoi(value);
                else if (strcmp(key, "bloom_min_age") == 0) current_species->bloom_min_age = atoi(value);
            }
        }
    }
    
    fclose(file);
    
    if (sim->species_count > 0) {
        printf("Successfully loaded %d species:\n", sim->species_count);
        for (int i = 0; i < sim->species_count; i++) {
            Species* s = &sim->species_list[i];
            printf("  %d: %s (growth: %.1f%%, RGB: %d,%d,%d)\n", 
                   i + 1, s->name, s->growth_rate * 100,
                   s->base_color.r, s->base_color.g, s->base_color.b);
        }
        return 1;
    }
    
    return 0;
}

// === SIMULATION MANAGEMENT ===
int init_simulation(Simulation* sim) {
    printf("Initializing simulation grid (%dx%d)...\n", GRID_WIDTH, GRID_HEIGHT);
    
    int grid_size = GRID_WIDTH * GRID_HEIGHT * sizeof(Cell);
    sim->grid = malloc(grid_size);
    sim->next_grid = malloc(grid_size);
    
    if (!sim->grid || !sim->next_grid) {
        printf("Error: Memory allocation failed\n");
        return 0;
    }
    
    reset_simulation(sim);
    return 1;
}

void reset_simulation(Simulation* sim) {
    int total_cells = GRID_WIDTH * GRID_HEIGHT;
    for (int i = 0; i < total_cells; i++) {
        sim->grid[i] = (Cell){CELL_EMPTY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        sim->next_grid[i] = sim->grid[i];
    }
    sim->frame_count = 0;
    sim->softbody_mode = 0;
    sim->cursor_radius = 4.0f;
    global_bloom_id = 1; // Reset bloom counter
}

void cleanup_simulation(Simulation* sim) {
    if (sim->grid) free(sim->grid);
    if (sim->next_grid) free(sim->next_grid);
    if (sim->species_list) free(sim->species_list);
    sim->grid = sim->next_grid = NULL;
    sim->species_list = NULL;
}

// === SOFTBODY DYNAMICS ===
int find_best_displacement_position(Simulation* sim, int original_x, int original_y, int push_from_x, int push_from_y) {
    // Calculate push direction
    int dx = original_x - push_from_x;
    int dy = original_y - push_from_y;
    
    // Find the best neighboring grid position to move to
    // Priority order: direct opposite direction, then adjacent directions
    int candidates[8][2] = {
        {dx > 0 ? 1 : (dx < 0 ? -1 : 0), dy > 0 ? 1 : (dy < 0 ? -1 : 0)}, // Primary direction
        {dx > 0 ? 1 : (dx < 0 ? -1 : 0), 0}, // Horizontal only
        {0, dy > 0 ? 1 : (dy < 0 ? -1 : 0)}, // Vertical only
        {dx > 0 ? 1 : (dx < 0 ? -1 : 0), dy > 0 ? -1 : (dy < 0 ? 1 : 0)}, // Perpendicular
        {dx > 0 ? -1 : (dx < 0 ? 1 : 0), dy > 0 ? 1 : (dy < 0 ? -1 : 0)}, // Other perpendicular
        {1, 0}, {-1, 0}, {0, 1}, {0, -1} // Cardinal directions as fallback
    };
    
    for (int i = 0; i < 8; i++) {
        int new_x = original_x + candidates[i][0];
        int new_y = original_y + candidates[i][1];
        
        // Check bounds
        if (new_x < 0 || new_x >= GRID_WIDTH || new_y < 0 || new_y >= GRID_HEIGHT) continue;
        
        // Check if position is empty or contains a nutrient (plants can push through nutrients)
        Cell* target = get_cell(sim, new_x, new_y);
        if (target && (target->state == CELL_EMPTY || target->state == CELL_NUTRIENT)) {
            return i; // Return the candidate index
        }
    }
    
    return -1; // No suitable position found
}

void apply_cursor_force(Simulation* sim, int cursor_x, int cursor_y) {
    if (!sim->softbody_mode) return;
    
    int influence_radius = (int)sim->cursor_radius;
    
    // First pass: identify cells to be pushed
    for (int y = cursor_y - influence_radius; y <= cursor_y + influence_radius; y++) {
        for (int x = cursor_x - influence_radius; x <= cursor_x + influence_radius; x++) {
            if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) continue;
            
            Cell* cell = get_cell(sim, x, y);
            if (!cell || cell->state != CELL_PLANT) continue;
            
            // Calculate distance from cursor
            int dx = x - cursor_x;
            int dy = y - cursor_y;
            int distance_squared = dx*dx + dy*dy;
            int radius_squared = influence_radius * influence_radius;
            
            if (distance_squared <= radius_squared && !(dx == 0 && dy == 0)) {
                // Store original position if not already displaced
                if (!cell->is_displaced) {
                    cell->original_x = x;
                    cell->original_y = y;
                    cell->is_displaced = 1;
                }
                
                // Calculate push strength based on distance (closer = stronger push)
                cell->push_strength = 10 - (int)(sqrt(distance_squared) * 3);
                if (cell->push_strength < 1) cell->push_strength = 1;
                
                // Find best displacement position
                int candidate_index = find_best_displacement_position(sim, x, y, cursor_x, cursor_y);
                
                if (candidate_index >= 0) {
                    // Move to the new position
                    int candidates[8][2] = {
                        {dx > 0 ? 1 : (dx < 0 ? -1 : 0), dy > 0 ? 1 : (dy < 0 ? -1 : 0)},
                        {dx > 0 ? 1 : (dx < 0 ? -1 : 0), 0},
                        {0, dy > 0 ? 1 : (dy < 0 ? -1 : 0)},
                        {dx > 0 ? 1 : (dx < 0 ? -1 : 0), dy > 0 ? -1 : (dy < 0 ? 1 : 0)},
                        {dx > 0 ? -1 : (dx < 0 ? 1 : 0), dy > 0 ? 1 : (dy < 0 ? -1 : 0)},
                        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
                    };
                    
                    cell->displaced_x = x + candidates[candidate_index][0];
                    cell->displaced_y = y + candidates[candidate_index][1];
                    
                    // Ensure displacement doesn't exceed maximum distance
                    int displacement_distance = abs(cell->displaced_x - cell->original_x) + 
                                              abs(cell->displaced_y - cell->original_y);
                    if (displacement_distance > MAX_DISPLACEMENT) {
                        // Limit displacement
                        int limit_dx = cell->displaced_x - cell->original_x;
                        int limit_dy = cell->displaced_y - cell->original_y;
                        
                        if (abs(limit_dx) > MAX_DISPLACEMENT) {
                            limit_dx = limit_dx > 0 ? MAX_DISPLACEMENT : -MAX_DISPLACEMENT;
                        }
                        if (abs(limit_dy) > MAX_DISPLACEMENT) {
                            limit_dy = limit_dy > 0 ? MAX_DISPLACEMENT : -MAX_DISPLACEMENT;
                        }
                        
                        cell->displaced_x = cell->original_x + limit_dx;
                        cell->displaced_y = cell->original_y + limit_dy;
                    }
                } else {
                    // Can't move - just set displaced position to current position
                    cell->displaced_x = x;
                    cell->displaced_y = y;
                }
            }
        }
    }
}

void update_softbody_physics(Simulation* sim) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            Cell* cell = get_cell(sim, x, y);
            if (!cell || cell->state != CELL_PLANT || !cell->is_displaced) continue;
            
            // Gradually reduce push strength
            if (cell->push_strength > 0) {
                cell->push_strength--;
            }
            
            // Spring back towards original position when no force is applied
            if (!sim->softbody_mode && cell->push_strength <= 0) {
                // Move one step closer to original position
                if (cell->displaced_x != cell->original_x || cell->displaced_y != cell->original_y) {
                    int dx = cell->original_x - cell->displaced_x;
                    int dy = cell->original_y - cell->displaced_y;
                    
                    // Move one grid step towards original position
                    if (abs(dx) >= abs(dy) && dx != 0) {
                        cell->displaced_x += dx > 0 ? 1 : -1;
                    } else if (dy != 0) {
                        cell->displaced_y += dy > 0 ? 1 : -1;
                    }
                    
                    // Check if we've returned to original position
                    if (cell->displaced_x == cell->original_x && cell->displaced_y == cell->original_y) {
                        cell->is_displaced = 0;
                        cell->push_strength = 0;
                    }
                }
            }
        }
    }
}
void create_bloom_cluster(Simulation* sim, int center_x, int center_y, int species_id) {
    Species* species = &sim->species_list[species_id];
    int bloom_id = global_bloom_id++;
    
    // Create bloom cluster around center point
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            Cell* cell = get_cell(sim, center_x + dx, center_y + dy);
            if (cell && cell->state == CELL_PLANT && cell->species_id == species_id) {
                cell->bloom_timer = species->bloom_duration;
                cell->bloom_id = bloom_id;
                if (dx == 0 && dy == 0) {
                    cell->is_bloom_center = 1;
                }
            }
        }
    }
}

void plant_seed(Simulation* sim, int x, int y, int species_id) {
    Cell* cell = get_cell(sim, x, y);
    if (cell && species_id >= 0 && species_id < sim->species_count) {
        cell->state = CELL_PLANT;
        cell->age = 0;
        cell->energy = sim->species_list[species_id].max_energy;
        cell->generation = 0;
        cell->species_id = species_id;
    }
}

void spawn_nutrients(Simulation* sim) {
    for (int attempts = 0; attempts < 3; attempts++) {
        int x = rand() % GRID_WIDTH;
        int y = rand() % GRID_HEIGHT;
        Cell* cell = get_cell(sim, x, y);
        
        if (cell && cell->state == CELL_EMPTY && (rand() % 1000) < 2) {
            cell->state = CELL_NUTRIENT;
            cell->energy = 25;
            cell->age = 0;
        }
    }
}

void setup_initial_ecosystem(Simulation* sim) {
    printf("Setting up initial ecosystem...\n");
    
    // Plant seeds evenly distributed
    for (int i = 0; i < sim->species_count; i++) {
        int x = (GRID_WIDTH / (sim->species_count + 1)) * (i + 1);
        int y = GRID_HEIGHT / 2;
        plant_seed(sim, x, y, i);
        
        // Surround each seed with nutrients
        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                if (dx == 0 && dy == 0) continue;
                Cell* cell = get_cell(sim, x + dx, y + dy);
                if (cell && cell->state == CELL_EMPTY && (rand() % 100) < 50) {
                    cell->state = CELL_NUTRIENT;
                    cell->energy = 25;
                    cell->age = 0;
                }
            }
        }
        
        printf("  Planted %s at (%d,%d)\n", sim->species_list[i].name, x, y);
    }
    
    // Add scattered nutrients
    for (int i = 0; i < 30; i++) {
        spawn_nutrients(sim);
    }
}

// === NEIGHBOR ANALYSIS ===
int count_neighbors_by_state(Simulation* sim, int x, int y, CellState state) {
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            Cell* neighbor = get_cell(sim, x + dx, y + dy);
            if (neighbor && neighbor->state == state) count++;
        }
    }
    return count;
}

void count_neighbors_by_species(Simulation* sim, int x, int y, int* species_counts) {
    memset(species_counts, 0, sim->species_count * sizeof(int));
    
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            Cell* neighbor = get_cell(sim, x + dx, y + dy);
            if (neighbor && neighbor->state == CELL_PLANT) {
                species_counts[neighbor->species_id]++;
            }
        }
    }
}

int absorb_nutrients(Simulation* sim, int x, int y, int radius) {
    int absorbed = 0;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx == 0 && dy == 0) continue;
            Cell* nutrient = get_cell(sim, x + dx, y + dy);
            
            if (nutrient && nutrient->state == CELL_NUTRIENT && nutrient->energy > 0) {
                int distance = abs(dx) + abs(dy);
                int absorption = nutrient->energy / (distance + 1);
                absorbed += absorption;
                nutrient->energy -= absorption;
                
                if (nutrient->energy <= 0) {
                    nutrient->state = CELL_EMPTY;
                    nutrient->energy = 0;
                }
            }
        }
    }
    
    return absorbed;
}

// === CELL UPDATE FUNCTIONS ===
void update_empty_cell(Simulation* sim, int x, int y, Cell* current, Cell* next) {
    int plant_neighbors = count_neighbors_by_state(sim, x, y, CELL_PLANT);
    int nutrient_neighbors = count_neighbors_by_state(sim, x, y, CELL_NUTRIENT);
    
    if (plant_neighbors >= 1 && nutrient_neighbors > 0) {
        int species_neighbors[MAX_SPECIES];
        count_neighbors_by_species(sim, x, y, species_neighbors);
        
        // Try growth for most dominant species first
        for (int attempts = 0; attempts < sim->species_count; attempts++) {
            int best_species = -1;
            int best_count = 0;
            
            for (int s = 0; s < sim->species_count; s++) {
                if (species_neighbors[s] > best_count) {
                    Species* species = &sim->species_list[s];
                    
                    // Standard neighbor-based growth
                    int can_grow_standard = (species_neighbors[s] >= species->min_neighbors && 
                                           species_neighbors[s] <= species->max_neighbors);
                    
                    // Branching growth - check for branching opportunities
                    int can_grow_branching = 0;
                    if (species->branching_factor > 0.0f) {
                        // Look for plants that can branch towards this position
                        for (int dy = -species->branching_distance; dy <= species->branching_distance; dy++) {
                            for (int dx = -species->branching_distance; dx <= species->branching_distance; dx++) {
                                if (dx == 0 && dy == 0) continue;
                                Cell* potential_parent = get_cell(sim, x + dx, y + dy);
                                if (potential_parent && potential_parent->state == CELL_PLANT && 
                                    potential_parent->species_id == s) {
                                    
                                    // Check if direction aligns with branching pattern
                                    int direction = (dx + 1) + (dy + 1) * 3; // 0-8 direction encoding
                                    int parent_dir = potential_parent->branch_direction;
                                    int dir_diff = abs(direction - parent_dir);
                                    
                                    if (dir_diff <= species->branching_angle_variance) {
                                        can_grow_branching = 1;
                                        break;
                                    }
                                }
                            }
                            if (can_grow_branching) break;
                        }
                    }
                    
                    if (can_grow_standard || can_grow_branching) {
                        best_species = s;
                        best_count = species_neighbors[s];
                    }
                }
            }
            
            if (best_species >= 0) {
                Species* species = &sim->species_list[best_species];
                
                // Check growth conditions again for the selected species
                int can_grow_standard = (species_neighbors[best_species] >= species->min_neighbors && 
                                       species_neighbors[best_species] <= species->max_neighbors);
                
                int can_grow_branching = 0;
                if (species->branching_factor > 0.0f) {
                    for (int dy = -species->branching_distance; dy <= species->branching_distance; dy++) {
                        for (int dx = -species->branching_distance; dx <= species->branching_distance; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            Cell* potential_parent = get_cell(sim, x + dx, y + dy);
                            if (potential_parent && potential_parent->state == CELL_PLANT && 
                                potential_parent->species_id == best_species) {
                                can_grow_branching = 1;
                                break;
                            }
                        }
                        if (can_grow_branching) break;
                    }
                }
                
                // Determine growth probability
                float growth_chance = species->growth_rate * 100;
                if (can_grow_branching) {
                    growth_chance *= (1.0f + species->branching_factor); // Boost for branching
                }
                
                if ((rand() % 100) < (int)growth_chance) {
                    next->state = CELL_PLANT;
                    next->age = 0;
                    next->energy = species->max_energy / 2;
                    next->generation = 0;
                    next->species_id = best_species;
                    next->bloom_timer = 0;
                    next->bloom_frames = 0;
                    next->branch_direction = rand() % 9; // Random initial direction
                    next->is_bloom_center = 0;
                    next->bloom_id = 0;
                    
                    absorb_nutrients(sim, x, y, species->nutrient_absorption_radius);
                    break;
                }
                species_neighbors[best_species] = 0; // Mark as tried
            } else {
                break;
            }
        }
    }
}

void update_plant_cell(Simulation* sim, int x, int y, Cell* current, Cell* next) {
    Species* species = &sim->species_list[current->species_id];
    
    // Slow aging system - only age every AGING_DIVISOR frames
    next->age_frames = current->age_frames + 1;
    if (next->age_frames >= AGING_DIVISOR) {
        next->age = current->age + 1;
        next->age_frames = 0;
    } else {
        next->age = current->age;
    }
    
    next->energy = current->energy - species->base_metabolism;
    next->energy += absorb_nutrients(sim, x, y, species->nutrient_absorption_radius);
    
    // Handle existing blooms (countdown)
    if (current->bloom_timer > 0) {
        next->bloom_timer = current->bloom_timer - 1;
        next->bloom_id = current->bloom_id;
        next->is_bloom_center = current->is_bloom_center;
        
        // If bloom expires, reset bloom properties
        if (next->bloom_timer == 0) {
            next->is_bloom_center = 0;
            next->bloom_id = 0;
        }
    } else {
        // Slow bloom timing - only check for blooms every AGING_DIVISOR frames
        next->bloom_frames = current->bloom_frames + 1;
        if (next->bloom_frames >= AGING_DIVISOR) {
            next->bloom_frames = 0;
            
            // Try to create new bloom if conditions are met
            if (current->age >= species->bloom_min_age && 
                current->bloom_timer == 0 && 
                !current->is_bloom_center &&
                (rand() % 10000) < (int)(species->bloom_probability * 10000)) {
                
                // Only create bloom if this area doesn't already have one
                int has_nearby_bloom = 0;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        Cell* nearby = get_cell(sim, x + dx, y + dy);
                        if (nearby && nearby->bloom_timer > 0) {
                            has_nearby_bloom = 1;
                            break;
                        }
                    }
                    if (has_nearby_bloom) break;
                }
                
                if (!has_nearby_bloom) {
                    create_bloom_cluster(sim, x, y, current->species_id);
                    next->bloom_timer = species->bloom_duration;
                    next->is_bloom_center = 1;
                    next->bloom_id = global_bloom_id - 1;
                }
            }
        } else {
            next->bloom_frames = current->bloom_frames;
        }
    }
    
    // Crowding stress
    int plant_neighbors = count_neighbors_by_state(sim, x, y, CELL_PLANT);
    if (plant_neighbors > species->max_crowding) {
        next->energy -= species->crowding_stress;
    }
    
    // Death conditions - use actual age
    if (next->age > species->death_age || next->energy <= 0) {
        next->state = CELL_DEAD_PLANT;
        next->energy = species->decompose_energy;
        next->age = 0;
        next->age_frames = 0;
        next->bloom_timer = 0;
        next->bloom_frames = 0;
        next->is_bloom_center = 0;
        next->bloom_id = 0;
    } else if (next->energy > species->max_energy) {
        next->energy = species->max_energy;
    }
}

void update_dead_plant_cell(Simulation* sim, int x, int y, Cell* current, Cell* next) {
    Species* species = &sim->species_list[current->species_id];
    
    next->age = current->age + 1;
    next->energy = current->energy - 2;
    
    if (next->energy <= 0 || next->age > species->decompose_time) {
        next->state = CELL_EMPTY;
        next->age = 0;
        next->energy = 0;
        next->generation = 0;
        next->species_id = 0;
    }
}

void update_nutrient_cell(Simulation* sim, int x, int y, Cell* current, Cell* next) {
    next->age = current->age + 1;
    next->energy = current->energy - 1;
    
    if (next->energy <= 0 || next->age > 50) {
        next->state = CELL_EMPTY;
        next->age = 0;
        next->energy = 0;
    }
}

void update_simulation(Simulation* sim) {
    sim->frame_count++;
    
    // Spawn nutrients periodically
    if (sim->frame_count % 15 == 0) {
        spawn_nutrients(sim);
    }
    
    // Update softbody physics
    update_softbody_physics(sim);
    
    // Apply cursor force if in softbody mode
    if (sim->softbody_mode) {
        apply_cursor_force(sim, sim->cursor_x, sim->cursor_y);
    }
    
    // Copy current state to next
    memcpy(sim->next_grid, sim->grid, GRID_WIDTH * GRID_HEIGHT * sizeof(Cell));
    
    // Update all cells
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            Cell* current = get_cell(sim, x, y);
            Cell* next = &sim->next_grid[get_grid_index(x, y)];
            
            switch (current->state) {
                case CELL_EMPTY: update_empty_cell(sim, x, y, current, next); break;
                case CELL_PLANT: update_plant_cell(sim, x, y, current, next); break;
                case CELL_DEAD_PLANT: update_dead_plant_cell(sim, x, y, current, next); break;
                case CELL_NUTRIENT: update_nutrient_cell(sim, x, y, current, next); break;
            }
        }
    }
    
    // Swap grids
    Cell* temp = sim->grid;
    sim->grid = sim->next_grid;
    sim->next_grid = temp;
}

// === RENDERING FUNCTIONS ===
Color calculate_plant_color(Simulation* sim, Cell* cell) {
    Species* species = &sim->species_list[cell->species_id];
    
    int energy_factor = (cell->energy * species->energy_brightness) / species->max_energy;
    int age_factor = species->age_blue_factor * (species->death_age - cell->age) / species->death_age;
    
    if (energy_factor < 0) energy_factor = 0;
    if (age_factor < 0) age_factor = 0;
    
    Color color = {
        species->base_color.r + cell->generation * species->generation_red_boost,
        species->base_color.g + energy_factor,
        species->base_color.b + age_factor
    };
    
    // Apply bloom effect if blooming
    if (cell->bloom_timer > 0) {
        float bloom_strength = (float)cell->bloom_timer / species->bloom_duration;
        int bloom_effect = (int)(bloom_strength * species->bloom_intensity);
        
        // Blend base color with bloom color
        color.r = (color.r * (100 - bloom_effect) + species->bloom_color.r * bloom_effect) / 100;
        color.g = (color.g * (100 - bloom_effect) + species->bloom_color.g * bloom_effect) / 100;
        color.b = (color.b * (100 - bloom_effect) + species->bloom_color.b * bloom_effect) / 100;
    }
    
    // Transition to death colors when energy is very low
    if (cell->energy <= 5) {
        int death_factor = (5 - cell->energy) * 20;
        color.r = (color.r * (100 - death_factor) + 40 * death_factor) / 100;
        color.g = (color.g * (100 - death_factor) + 25 * death_factor) / 100;
        color.b = (color.b * (100 - death_factor) + 10 * death_factor) / 100;
    }
    
    // Clamp values
    if (color.r > 255) color.r = 255;
    if (color.g > 255) color.g = 255;
    if (color.b > 255) color.b = 255;
    
    return color;
}

Color calculate_dead_plant_color(Simulation* sim, Cell* cell) {
    Species* species = &sim->species_list[cell->species_id];
    int decay = (cell->energy * 60) / species->decompose_energy;
    if (decay > 60) decay = 60;
    
    return (Color){decay + 20, decay / 2 + 10, decay / 4};
}

Color calculate_nutrient_color(Cell* cell) {
    int intensity = (cell->energy * 180) / 25;
    return (Color){intensity * 60 / 180, intensity * 60 / 180, intensity};
}

void render_simulation(Simulation* sim) {
    SDL_SetRenderDrawColor(sim->renderer, 3, 3, 8, 255);
    SDL_RenderClear(sim->renderer);
    
    // Create a temporary render map to handle overlapping displaced cells
    int render_occupied[GRID_HEIGHT][GRID_WIDTH];
    memset(render_occupied, 0, sizeof(render_occupied));
    
    // First pass: mark occupied positions and render cells
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            Cell* cell = get_cell(sim, x, y);
            if (!cell || cell->state == CELL_EMPTY) continue;
            
            // Determine render position
            int render_x = x;
            int render_y = y;
            
            if (cell->is_displaced) {
                render_x = cell->displaced_x;
                render_y = cell->displaced_y;
                
                // Ensure render position is within bounds
                if (render_x < 0) render_x = 0;
                if (render_x >= GRID_WIDTH) render_x = GRID_WIDTH - 1;
                if (render_y < 0) render_y = 0;
                if (render_y >= GRID_HEIGHT) render_y = GRID_HEIGHT - 1;
            }
            
            // Skip if position is already occupied by another displaced cell
            if (render_occupied[render_y][render_x]) continue;
            render_occupied[render_y][render_x] = 1;
            
            Color color = {8, 8, 15};
            switch (cell->state) {
                case CELL_PLANT: 
                    color = calculate_plant_color(sim, cell);
                    // Add visual feedback for pushed plants
                    if (cell->push_strength > 0) {
                        color.r = (color.r + 50 > 255) ? 255 : color.r + 50; // Brighten red
                        color.g = (color.g + 30 > 255) ? 255 : color.g + 30; // Brighten green
                    }
                    break;
                case CELL_DEAD_PLANT: 
                    color = calculate_dead_plant_color(sim, cell); 
                    break;
                case CELL_NUTRIENT: 
                    color = calculate_nutrient_color(cell); 
                    break;
                default: 
                    continue;
            }
            
            SDL_SetRenderDrawColor(sim->renderer, color.r, color.g, color.b, 255);
            SDL_Rect rect = {render_x * PIXEL_SIZE, render_y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE};
            SDL_RenderFillRect(sim->renderer, &rect);
        }
    }
    
    // Draw cursor indicator when in softbody mode
    if (sim->softbody_mode) {
        SDL_SetRenderDrawColor(sim->renderer, 255, 255, 0, 150);
        
        // Draw cursor position as a cross
        int cursor_pixel_x = sim->cursor_x * PIXEL_SIZE;
        int cursor_pixel_y = sim->cursor_y * PIXEL_SIZE;
        
        SDL_Rect cursor_h = {cursor_pixel_x - 3, cursor_pixel_y, 7, 1};
        SDL_Rect cursor_v = {cursor_pixel_x, cursor_pixel_y - 3, 1, 7};
        SDL_RenderFillRect(sim->renderer, &cursor_h);
        SDL_RenderFillRect(sim->renderer, &cursor_v);
        
        // Draw influence radius as a grid-aligned square
        int radius = (int)sim->cursor_radius;
        SDL_SetRenderDrawColor(sim->renderer, 255, 255, 0, 50);
        
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx*dx + dy*dy <= radius*radius) {
                    int grid_x = sim->cursor_x + dx;
                    int grid_y = sim->cursor_y + dy;
                    
                    if (grid_x >= 0 && grid_x < GRID_WIDTH && grid_y >= 0 && grid_y < GRID_HEIGHT) {
                        SDL_Rect influence_rect = {
                            grid_x * PIXEL_SIZE, 
                            grid_y * PIXEL_SIZE, 
                            PIXEL_SIZE, 
                            PIXEL_SIZE
                        };
                        SDL_RenderDrawRect(sim->renderer, &influence_rect);
                    }
                }
            }
        }
    }
    
    SDL_RenderPresent(sim->renderer);
}

// === INPUT HANDLING ===
void handle_events(Simulation* sim) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                sim->running = 0;
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    const Uint8* keystate = SDL_GetKeyboardState(NULL);
                    
                    if (keystate[SDL_SCANCODE_LCTRL] || keystate[SDL_SCANCODE_RCTRL]) {
                        // Ctrl+Left Click - Enter softbody mode
                        sim->softbody_mode = 1;
                        sim->cursor_x = event.button.x / PIXEL_SIZE;
                        sim->cursor_y = event.button.y / PIXEL_SIZE;
                        sim->cursor_radius = 4.0f; // Adjustable influence radius
                        printf("Softbody mode activated - push plants around!\n");
                    } else {
                        // Normal left click - plant seed
                        sim->mouse_pressed = 1;
                        int grid_x = event.button.x / PIXEL_SIZE;
                        int grid_y = event.button.y / PIXEL_SIZE;
                        plant_seed(sim, grid_x, grid_y, sim->current_species);
                    }
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    int grid_x = event.button.x / PIXEL_SIZE;
                    int grid_y = event.button.y / PIXEL_SIZE;
                    for (int dy = -2; dy <= 2; dy++) {
                        for (int dx = -2; dx <= 2; dx++) {
                            Cell* cell = get_cell(sim, grid_x + dx, grid_y + dy);
                            if (cell && cell->state == CELL_EMPTY && (rand() % 100) < 50) {
                                cell->state = CELL_NUTRIENT;
                                cell->energy = 25;
                                cell->age = 0;
                            }
                        }
                    }
                }
                break;
                
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (sim->softbody_mode) {
                        sim->softbody_mode = 0;
                        printf("Softbody mode deactivated - plants will spring back\n");
                    } else {
                        sim->mouse_pressed = 0;
                    }
                }
                break;
                
            case SDL_MOUSEMOTION:
                if (sim->softbody_mode) {
                    // Update cursor position in softbody mode
                    sim->cursor_x = event.motion.x / PIXEL_SIZE;
                    sim->cursor_y = event.motion.y / PIXEL_SIZE;
                } else if (sim->mouse_pressed) {
                    // Normal mouse drag planting
                    int grid_x = event.motion.x / PIXEL_SIZE;
                    int grid_y = event.motion.y / PIXEL_SIZE;
                    plant_seed(sim, grid_x, grid_y, sim->current_species);
                }
                break;
                
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_r) {
                    reset_simulation(sim);
                    setup_initial_ecosystem(sim);
                    printf("Simulation reset\n");
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    plant_seed(sim, GRID_WIDTH/2, GRID_HEIGHT/2, sim->current_species);
                    printf("Planted %s at center\n", sim->species_list[sim->current_species].name);
                } else if (event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_9) {
                    int species_num = event.key.keysym.sym - SDLK_1;
                    if (species_num < sim->species_count) {
                        sim->current_species = species_num;
                        printf("Selected: %s\n", sim->species_list[species_num].name);
                    }
                } else if (event.key.keysym.sym == SDLK_TAB) {
                    sim->current_species = (sim->current_species + 1) % sim->species_count;
                    printf("Selected: %s\n", sim->species_list[sim->current_species].name);
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    sim->running = 0;
                } else if (event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_KP_PLUS) {
                    // Increase cursor radius
                    if (sim->cursor_radius < 10.0f) {
                        sim->cursor_radius += 0.5f;
                        printf("Cursor radius: %.1f\n", sim->cursor_radius);
                    }
                } else if (event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_KP_MINUS) {
                    // Decrease cursor radius
                    if (sim->cursor_radius > 1.0f) {
                        sim->cursor_radius -= 0.5f;
                        printf("Cursor radius: %.1f\n", sim->cursor_radius);
                    }
                }
                break;
        }
    }
}

// === INFORMATION DISPLAY ===
void print_species_info(Simulation* sim) {
    printf("\nLoaded Species:\n");
    for (int i = 0; i < sim->species_count; i++) {
        Species* s = &sim->species_list[i];
        printf("  %d: %s\n", i + 1, s->name);
        printf("     Growth: %.1f%%, Neighbors: %d-%d, Lifespan: %d\n",
               s->growth_rate * 100, s->min_neighbors, s->max_neighbors, s->death_age);
        printf("     Color: RGB(%d,%d,%d)\n", 
               s->base_color.r, s->base_color.g, s->base_color.b);
    }
}

void print_controls(Simulation* sim) {
    printf("\nControls:\n");
    printf("  Left Click/Drag: Plant current species\n");
    printf("  Right Click: Add nutrients\n");
    printf("  1-%d: Select species\n", sim->species_count);
    printf("  TAB: Cycle through species\n");
    printf("  SPACE: Plant at center\n");
    printf("  R: Reset simulation\n");
  printf("  ESC: Exit\n");
  printf("\nCurrent species: %s\n", sim->species_list[sim->current_species].name);
}

// === MAIN PROGRAM ===
int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));
    setup_console();
    
    printf("=== MODULAR PLANT ECOSYSTEM SIMULATION ===\n");
    printf("Loading configuration...\n");
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        printf("Press Enter to exit...\n");
        getchar();
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow("Plant Ecosystem Simulation",
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window Error: %s\n", SDL_GetError());
        SDL_Quit();
        printf("Press Enter to exit...\n");
        getchar();
        return 1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        printf("Press Enter to exit...\n");
        getchar();
        return 1;
    }
    
    printf("Graphics initialized successfully\n");
    
    // Initialize simulation
    Simulation sim = {0};
    sim.renderer = renderer;
    sim.running = 1;
    sim.current_species = 0;
    
    // Load species configuration
    if (!load_species_config(&sim, CONFIG_FILE)) {
        printf("\nNo valid %s found. Please create configuration file.\n", CONFIG_FILE);
        printf("Example format:\n\n");
        printf("algae:\n");
        printf("  growth_rate: 0.35\n");
        printf("  min_neighbors: 2\n");
        printf("  max_neighbors: 3\n");
        printf("  base_color:\n");
        printf("    r: 20\n");
        printf("    g: 150\n");
        printf("    b: 30\n");
        printf("  # ... other properties\n\n");
        
        cleanup_simulation(&sim);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        
#ifdef _WIN32
        printf("Press Enter to exit...\n");
        getchar();
        FreeConsole();
#endif
        return 1;
    }
    
    // Initialize grid and ecosystem
    if (!init_simulation(&sim)) {
        printf("Failed to initialize simulation\n");
        cleanup_simulation(&sim);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    setup_initial_ecosystem(&sim);
    
    // Print information
    print_species_info(&sim);
    print_controls(&sim);
    
    printf("\n=== SIMULATION STARTED ===\n");
    printf("Grid: %dx%d cells, Window: %dx%d pixels\n", 
           GRID_WIDTH, GRID_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    // Main simulation loop
    while (sim.running) {
        handle_events(&sim);
        update_simulation(&sim);
        render_simulation(&sim);
        SDL_Delay(80); // ~12 FPS
        
        // Status updates
        if (sim.frame_count % 100 == 0) {
            int total_plants = 0;
            int species_counts[MAX_SPECIES] = {0};
            
            for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
                if (sim.grid[i].state == CELL_PLANT) {
                    total_plants++;
                    species_counts[sim.grid[i].species_id]++;
                }
            }
            
            printf("Frame %d: %d total plants (", sim.frame_count, total_plants);
            for (int i = 0; i < sim.species_count; i++) {
                printf("%s:%d%s", sim.species_list[i].name, species_counts[i],
                       i < sim.species_count - 1 ? ", " : "");
            }
            printf(")\n");
        }
    }
    
    printf("Simulation ended\n");
    
    // Cleanup
    cleanup_simulation(&sim);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
#ifdef _WIN32
    printf("Press Enter to close...\n");
    getchar();
    FreeConsole();
#endif
    
    return 0;
}