#include <SDL2/SDL.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "types.h"
#include "simulation.h"
#include "camera.h"
#include "plants.h"
#include "physics.h"
#include "rendering.h"
#include "nutrition.h"

static int g_current_plant_type = 0;

static void populate_reef_randomly(void) {
    int total_species = plants_get_type_count();
    if (total_species == 0) return;
    
    // Spawn initial reef population across the world
    int spawn_count = 500; // Total initial organisms
    
    printf("Populating reef with %d organisms across %d species...\n", spawn_count, total_species);
    
    for (int i = 0; i < spawn_count; i++) {
        // Random position across the world
        float x = WORLD_LEFT + ((float)rand() / RAND_MAX) * WORLD_WIDTH;
        float y = WORLD_TOP + ((float)rand() / RAND_MAX) * WORLD_HEIGHT;
        
        // Random species selection
        int species = rand() % total_species;
        
        // Create organism
        int node_id = simulation_add_node(x, y, species);
        if (node_id >= 0) {
            PlantType* pt = plants_get_type(species);
            if (pt) {
                printf("Spawned %s at (%.0f, %.0f)\n", pt->name, x, y);
            }
        }
    }
    
    printf("Reef population complete!\n");
}

static void handle_mouse_click(int screen_x, int screen_y, int button) {
    float world_x, world_y;
    camera_screen_to_world(screen_x, screen_y, &world_x, &world_y);
    
    if (button == SDL_BUTTON_LEFT) {
        // Create new node with current plant type
        if (plants_get_type_count() > 0) {
            int new_node = simulation_add_node(world_x, world_y, g_current_plant_type);
            if (new_node >= 0) {
                PlantType* pt = plants_get_type(g_current_plant_type);
                printf("Created %s node %d at (%.1f, %.1f)\n", 
                       pt->name, new_node, world_x, world_y);
            }
        }
    } else if (button == SDL_BUTTON_RIGHT) {
        // Node selection for chaining
        int clicked_node = simulation_find_node_at_position(world_x, world_y);
        
        if (clicked_node >= 0) {
            int selection_mode = simulation_get_selection_mode();
            int selected_node = simulation_get_selected_node();
            
            if (selection_mode == 0) {
                simulation_set_selected_node(clicked_node);
                printf("Selected node %d for chaining\n", clicked_node);
            } else if (selection_mode == 1) {
                if (clicked_node != selected_node) {
                    int chain_id = simulation_add_chain(selected_node, clicked_node);
                    if (chain_id >= 0) {
                        printf("Created chain between nodes %d and %d\n", selected_node, clicked_node);
                    }
                }
                simulation_clear_selection();
            }
        } else {
            simulation_clear_selection();
        }
    }
}

int main(int argc, char* argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    srand((unsigned int)time(NULL));
    
    // Load plant configuration
    if (!plants_load_config("items.conf")) {
        printf("Error: No plant types loaded. Please create items.conf file.\n");
        return 1;
    }
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow("Great Barrier Reef Ecosystem",
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
    
    // Enable alpha blending for nutrition layer
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // Initialize systems
    if (!simulation_init() || !camera_init() || !rendering_init(renderer) || !nutrition_init()) {
        printf("System initialization failed\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Populate reef with random organisms
    populate_reef_randomly();
    
    int running = 1;
    SDL_Event event;
    int mouse_x = 0, mouse_y = 0;
    
    printf("\nGreat Barrier Reef Ecosystem Controls:\n");
    printf("  Left click: Create organism (current: %s)\n", 
           plants_get_type_count() > 0 ? plants_get_type(g_current_plant_type)->name : "none");
    printf("  Right click: Select nodes for chaining\n");
    printf("  WASD: Move camera\n");
    printf("  Shift + WASD: Sprint (2x speed)\n");
    printf("  Mouse wheel: Zoom in/out\n");
    printf("  1-8: Switch species type\n");
    printf("  N: Toggle nutrition layer (rainbow)\n");
    printf("  R: Repopulate reef randomly\n");
    printf("  ESC: Exit\n\n");
    
    while (running) {
        // Get current keyboard state for smooth movement
        const Uint8* keyboard_state = SDL_GetKeyboardState(NULL);
        
        // Movement keys
        int keys[4] = {0};
        keys[0] = keyboard_state[SDL_SCANCODE_W];
        keys[1] = keyboard_state[SDL_SCANCODE_A];
        keys[2] = keyboard_state[SDL_SCANCODE_S];
        keys[3] = keyboard_state[SDL_SCANCODE_D];
        
        // Sprint detection
        int sprint_active = keyboard_state[SDL_SCANCODE_LSHIFT] || keyboard_state[SDL_SCANCODE_RSHIFT];
        
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    else if (event.key.keysym.sym == SDLK_n) {
                        nutrition_toggle_visibility();
                    }
                    else if (event.key.keysym.sym == SDLK_r) {
                        // Repopulate reef
                        populate_reef_randomly();
                    }
                    // Species selection
                    else if (event.key.keysym.sym >= SDLK_1 && event.key.keysym.sym <= SDLK_8) {
                        int species_index = event.key.keysym.sym - SDLK_1;
                        if (species_index < plants_get_type_count()) {
                            g_current_plant_type = species_index;
                            printf("Selected species: %s%s\n", 
                                   plants_get_type(g_current_plant_type)->name,
                                   sprint_active ? " (SPRINT MODE)" : "");
                        }
                    }
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    handle_mouse_click(event.button.x, event.button.y, event.button.button);
                    break;
                    
                case SDL_MOUSEWHEEL:
                    SDL_GetMouseState(&mouse_x, &mouse_y);
                    if (event.wheel.y > 0) {
                        camera_zoom(ZOOM_SPEED, mouse_x, mouse_y);
                    } else if (event.wheel.y < 0) {
                        camera_zoom(-ZOOM_SPEED, mouse_x, mouse_y);
                    }
                    break;
                    
                case SDL_MOUSEMOTION:
                    mouse_x = event.motion.x;
                    mouse_y = event.motion.y;
                    break;
            }
        }
        
        // Update camera with sprint
        camera_update_with_sprint(keys, sprint_active);
        physics_update();
        rendering_render();
        
        SDL_Delay(16);
    }
    
    // Cleanup systems
    nutrition_cleanup();
    simulation_cleanup();
    rendering_cleanup();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}