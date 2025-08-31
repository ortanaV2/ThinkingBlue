#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define PIXEL_SIZE 4              // Size of each "pixel" cell on screen
#define GRID_WIDTH (WINDOW_WIDTH / PIXEL_SIZE)
#define GRID_HEIGHT (WINDOW_HEIGHT / PIXEL_SIZE)

// Pixel states
#define EMPTY 0
#define ALGAE 1
#define DEAD_ALGAE 2
#define NUTRIENT 3

// Growth parameters
#define MAX_AGE 100
#define NUTRIENT_SPAWN_RATE 0.001f
#define GROWTH_RATE 0.3f
#define DEATH_AGE 80

typedef struct {
    float x, y;
} Point;

typedef struct {
    int state;          // EMPTY, ALGAE, DEAD_ALGAE, NUTRIENT
    int age;           // How long this pixel has been alive
    float energy;      // Energy level for growth
    int generation;    // Distance from original seed
} Pixel;

typedef struct {
    Pixel grid[GRID_HEIGHT][GRID_WIDTH];
    Pixel next_grid[GRID_HEIGHT][GRID_WIDTH];  // For simultaneous updates
    SDL_Renderer* renderer;
    int running;
    float global_timer;
    int frame_count;
    Point mouse_pos;
    int mouse_pressed;
} Simulation;

// Initialize empty simulation
void init_simulation(Simulation* sim) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            sim->grid[y][x].state = EMPTY;
            sim->grid[y][x].age = 0;
            sim->grid[y][x].energy = 0;
            sim->grid[y][x].generation = 0;
            
            sim->next_grid[y][x] = sim->grid[y][x];
        }
    }
}

// Plant initial algae seed
void plant_algae_seed(Simulation* sim, int x, int y) {
    if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
        sim->grid[y][x].state = ALGAE;
        sim->grid[y][x].age = 0;
        sim->grid[y][x].energy = 100.0f;
        sim->grid[y][x].generation = 0;
    }
}

// Spawn random nutrients
void spawn_nutrients(Simulation* sim) {
    if ((rand() % 1000) / 1000.0f < NUTRIENT_SPAWN_RATE) {
        int x = rand() % GRID_WIDTH;
        int y = rand() % GRID_HEIGHT;
        
        if (sim->grid[y][x].state == EMPTY) {
            sim->grid[y][x].state = NUTRIENT;
            sim->grid[y][x].energy = 20.0f;
        }
    }
}

// Count neighbors of specific type
int count_neighbors(Simulation* sim, int x, int y, int state_type) {
    int count = 0;
    
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue; // Skip self
            
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                if (sim->grid[ny][nx].state == state_type) {
                    count++;
                }
            }
        }
    }
    
    return count;
}

// Get average generation of algae neighbors
float get_neighbor_generation(Simulation* sim, int x, int y) {
    float total_gen = 0;
    int algae_count = 0;
    
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                if (sim->grid[ny][nx].state == ALGAE) {
                    total_gen += sim->grid[ny][nx].generation;
                    algae_count++;
                }
            }
        }
    }
    
    return algae_count > 0 ? total_gen / algae_count : 0;
}

// Absorb nearby nutrients
float absorb_nutrients(Simulation* sim, int x, int y) {
    float absorbed = 0;
    
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                if (sim->grid[ny][nx].state == NUTRIENT) {
                    float distance = sqrt(dx*dx + dy*dy);
                    float absorption = sim->grid[ny][nx].energy / (distance + 1);
                    absorbed += absorption;
                    
                    sim->grid[ny][nx].energy -= absorption;
                    if (sim->grid[ny][nx].energy <= 0) {
                        sim->grid[ny][nx].state = EMPTY;
                        sim->grid[ny][nx].energy = 0;
                    }
                }
            }
        }
    }
    
    return absorbed;
}

// Update a single pixel according to algae growth rules
void update_pixel(Simulation* sim, int x, int y) {
    Pixel* current = &sim->grid[y][x];
    Pixel* next = &sim->next_grid[y][x];
    
    // Copy current state as default
    *next = *current;
    
    switch (current->state) {
        case EMPTY:
            {
                int algae_neighbors = count_neighbors(sim, x, y, ALGAE);
                int nutrient_neighbors = count_neighbors(sim, x, y, NUTRIENT);
                
                // Growth conditions: 2-3 algae neighbors + nutrients available
                if (algae_neighbors >= 2 && algae_neighbors <= 3 && nutrient_neighbors > 0) {
                    if ((rand() % 1000) / 1000.0f < GROWTH_RATE) {
                        next->state = ALGAE;
                        next->age = 0;
                        next->energy = 50.0f;
                        next->generation = (int)(get_neighbor_generation(sim, x, y) + 1);
                        
                        // Consume some nearby nutrients
                        absorb_nutrients(sim, x, y);
                    }
                }
            }
            break;
            
        case ALGAE:
            {
                next->age = current->age + 1;
                
                // Absorb nutrients to maintain energy
                next->energy = current->energy + absorb_nutrients(sim, x, y) - 1.0f; // Base metabolism
                
                // Overcrowding death
                int algae_neighbors = count_neighbors(sim, x, y, ALGAE);
                if (algae_neighbors > 5) {
                    next->energy -= 5.0f; // Stress from overcrowding
                }
                
                // Age-based death
                if (current->age > DEATH_AGE || next->energy <= 0) {
                    next->state = DEAD_ALGAE;
                    next->energy = 30.0f; // Decomposing algae provides some energy
                    next->age = 0;
                }
                
                // Energy cap
                if (next->energy > 100.0f) {
                    next->energy = 100.0f;
                }
            }
            break;
            
        case DEAD_ALGAE:
            {
                next->age = current->age + 1;
                next->energy = current->energy - 1.0f; // Decomposition
                
                // Decomposed completely
                if (next->energy <= 0 || current->age > 20) {
                    next->state = EMPTY;
                    next->age = 0;
                    next->energy = 0;
                    next->generation = 0;
                }
            }
            break;
            
        case NUTRIENT:
            {
                next->age = current->age + 1;
                next->energy = current->energy - 0.5f; // Nutrients decay slowly
                
                // Nutrient depleted
                if (next->energy <= 0 || current->age > 50) {
                    next->state = EMPTY;
                    next->age = 0;
                    next->energy = 0;
                    next->generation = 0;
                }
            }
            break;
    }
}

// Update entire simulation
void update_simulation(Simulation* sim) {
    sim->global_timer += 1.0f;
    sim->frame_count++;
    
    // Spawn nutrients periodically
    if (sim->frame_count % 10 == 0) { // Every 10 frames
        for (int i = 0; i < 5; i++) { // Spawn multiple nutrients
            spawn_nutrients(sim);
        }
    }
    
    // Update all pixels simultaneously (cellular automaton style)
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            update_pixel(sim, x, y);
        }
    }
    
    // Copy next_grid to current grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            sim->grid[y][x] = sim->next_grid[y][x];
        }
    }
}

// Get color for pixel based on state and properties
SDL_Color get_pixel_color(Pixel* pixel) {
    SDL_Color color = {0, 0, 0, 255}; // Default black
    
    switch (pixel->state) {
        case EMPTY:
            color.r = 5; color.g = 5; color.b = 15; // Dark space
            break;
            
        case ALGAE:
            {
                // Color based on age and energy
                float age_factor = 1.0f - (pixel->age / (float)MAX_AGE);
                float energy_factor = pixel->energy / 100.0f;
                
                color.r = (Uint8)(30 + pixel->generation * 10); // More red with generation
                color.g = (Uint8)(80 + energy_factor * 100);    // Brighter green with more energy
                color.b = (Uint8)(20 + age_factor * 40);        // More blue when young
                
                // Clamp values
                if (color.r > 255) color.r = 255;
                if (color.g > 255) color.g = 255;
                if (color.b > 255) color.b = 255;
            }
            break;
            
        case DEAD_ALGAE:
            {
                float decay = pixel->energy / 30.0f;
                color.r = (Uint8)(60 * decay);
                color.g = (Uint8)(40 * decay);
                color.b = (Uint8)(20 * decay);
            }
            break;
            
        case NUTRIENT:
            {
                float intensity = pixel->energy / 20.0f;
                color.r = (Uint8)(100 * intensity);
                color.g = (Uint8)(100 * intensity);
                color.b = (Uint8)(200 * intensity); // Bluish nutrients
            }
            break;
    }
    
    return color;
}

// Render the simulation
void render_simulation(Simulation* sim) {
    // Clear screen
    SDL_SetRenderDrawColor(sim->renderer, 2, 2, 8, 255);
    SDL_RenderClear(sim->renderer);
    
    // Draw each pixel
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            Pixel* pixel = &sim->grid[y][x];
            
            if (pixel->state != EMPTY) {
                SDL_Color color = get_pixel_color(pixel);
                SDL_SetRenderDrawColor(sim->renderer, color.r, color.g, color.b, 255);
                
                SDL_Rect rect = {
                    x * PIXEL_SIZE,
                    y * PIXEL_SIZE,
                    PIXEL_SIZE,
                    PIXEL_SIZE
                };
                SDL_RenderFillRect(sim->renderer, &rect);
            }
        }
    }
    
    // Draw statistics
    SDL_SetRenderDrawColor(sim->renderer, 100, 150, 200, 255);
    
    // Count different pixel types
    int algae_count = 0, nutrient_count = 0, dead_count = 0;
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            switch (sim->grid[y][x].state) {
                case ALGAE: algae_count++; break;
                case NUTRIENT: nutrient_count++; break;
                case DEAD_ALGAE: dead_count++; break;
            }
        }
    }
    
    // Simple visual stats (dots)
    for (int i = 0; i < algae_count / 10 && i < 100; i++) {
        SDL_RenderDrawPoint(sim->renderer, 10 + i, 10); // Algae count
    }
    for (int i = 0; i < nutrient_count / 5 && i < 100; i++) {
        SDL_RenderDrawPoint(sim->renderer, 10 + i, 15); // Nutrient count
    }
    for (int i = 0; i < dead_count / 5 && i < 100; i++) {
        SDL_RenderDrawPoint(sim->renderer, 10 + i, 20); // Dead count
    }
    
    SDL_RenderPresent(sim->renderer);
}

void handle_events(Simulation* sim) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                sim->running = 0;
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    sim->mouse_pressed = 1;
                    sim->mouse_pos.x = event.button.x;
                    sim->mouse_pos.y = event.button.y;
                    
                    // Plant algae at mouse position
                    int grid_x = event.button.x / PIXEL_SIZE;
                    int grid_y = event.button.y / PIXEL_SIZE;
                    plant_algae_seed(sim, grid_x, grid_y);
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    // Spawn nutrients at mouse position
                    int grid_x = event.button.x / PIXEL_SIZE;
                    int grid_y = event.button.y / PIXEL_SIZE;
                    
                    for (int dy = -2; dy <= 2; dy++) {
                        for (int dx = -2; dx <= 2; dx++) {
                            int nx = grid_x + dx;
                            int ny = grid_y + dy;
                            if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                                if (sim->grid[ny][nx].state == EMPTY && (rand() % 100) < 30) {
                                    sim->grid[ny][nx].state = NUTRIENT;
                                    sim->grid[ny][nx].energy = 20.0f;
                                }
                            }
                        }
                    }
                }
                break;
                
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    sim->mouse_pressed = 0;
                }
                break;
                
            case SDL_MOUSEMOTION:
                if (sim->mouse_pressed) {
                    // Continuous planting while dragging
                    int grid_x = event.motion.x / PIXEL_SIZE;
                    int grid_y = event.motion.y / PIXEL_SIZE;
                    plant_algae_seed(sim, grid_x, grid_y);
                }
                break;
                
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_r) {
                    // Reset simulation
                    init_simulation(sim);
                    printf("Simulation reset\n");
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    // Plant algae in center
                    plant_algae_seed(sim, GRID_WIDTH/2, GRID_HEIGHT/2);
                }
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));
    
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow("Pixelbasierte Algen-Wachstumssimulation",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        WINDOW_WIDTH, WINDOW_HEIGHT,
                                        SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    Simulation sim = {0};
    sim.renderer = renderer;
    sim.running = 1;
    sim.global_timer = 0.0f;
    sim.frame_count = 0;
    sim.mouse_pressed = 0;
    
    init_simulation(&sim);
    
    printf("Pixelbasierte Algen-Wachstumssimulation\n");
    printf("Grid: %dx%d Pixel (jeweils %dx%d Bildschirmpixel)\n", GRID_WIDTH, GRID_HEIGHT, PIXEL_SIZE, PIXEL_SIZE);
    printf("\nWachstumsregeln:\n");
    printf("- Algen brauchen 2-3 Nachbarn + Nährstoffe zum Wachsen\n");
    printf("- Überbevölkerung (>5 Nachbarn) führt zu Stress\n");
    printf("- Algen altern und sterben, werden zu Kompost\n");
    printf("- Nährstoffe spawnen zufällig und werden verbraucht\n");
    printf("\nSteuerung:\n");
    printf("- Linksklick/Drag: Algen pflanzen\n");
    printf("- Rechtsklick: Nährstoffe hinzufügen\n");
    printf("- SPACE: Alge im Zentrum pflanzen\n");
    printf("- R: Simulation zurücksetzen\n");
    printf("\nFarben:\n");
    printf("- Grün: Lebende Algen (heller = mehr Energie)\n");
    printf("- Braun: Tote/verwesende Algen\n");
    printf("- Blau: Nährstoffe\n");
    
    // Plant initial algae seed
    plant_algae_seed(&sim, GRID_WIDTH/2, GRID_HEIGHT/2);
    
    // Add some initial nutrients
    for (int i = 0; i < 20; i++) {
        spawn_nutrients(&sim);
    }
    
    while (sim.running) {
        handle_events(&sim);
        update_simulation(&sim);
        render_simulation(&sim);
        SDL_Delay(100); // Slower update for better observation
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}