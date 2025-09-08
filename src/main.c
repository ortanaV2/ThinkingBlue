#include <SDL2/SDL.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "types.h"
#include "simulation.h"
#include "camera.h"
#include "plants.h"
#include "fish.h"
#include "python_api.h"
#include "physics.h"
#include "rendering.h"
#include "nutrition.h"
#include "gas.h"

#define TARGET_FPS 30
#define FRAME_DELAY (1000 / TARGET_FPS)

// Global state
static int g_current_plant_type = 0;
static int g_current_fish_type = 0;
static int g_spawn_mode = 0; // 0 = plants, 1 = fish

static void populate_reef_randomly(void) {
    int total_plant_species = plants_get_type_count();
    int total_fish_species = fish_get_type_count();
    
    if (total_plant_species == 0) return;
    
    printf("Populating reef with plants and fish...\n");
    
    // Spawn 300 plants
    for (int i = 0; i < 300; i++) {
        float x = WORLD_LEFT + ((float)rand() / RAND_MAX) * WORLD_WIDTH;
        float y = WORLD_TOP + ((float)rand() / RAND_MAX) * WORLD_HEIGHT;
        int species = rand() % total_plant_species;
        simulation_add_node(x, y, species);
    }
    
    // Spawn fish if available
    if (total_fish_species > 0) {
        int fish_count = 20; // Spawn 20 fish
        printf("Spawning %d fish...\n", fish_count);
        
        for (int i = 0; i < fish_count; i++) {
            float x = WORLD_LEFT + ((float)rand() / RAND_MAX) * WORLD_WIDTH;
            float y = WORLD_TOP + ((float)rand() / RAND_MAX) * WORLD_HEIGHT;
            int fish_type = rand() % total_fish_species;
            
            int fish_id = fish_add(x, y, fish_type);
            if (fish_id >= 0) {
                FishType* ft = fish_get_type(fish_type);
                printf("Spawned %s at (%.0f, %.0f)\n", ft->name, x, y);
            }
        }
    }
    
    printf("Reef population complete!\n");
}

static void handle_mouse_click(int screen_x, int screen_y, int button) {
    float world_x, world_y;
    camera_screen_to_world(screen_x, screen_y, &world_x, &world_y);
    
    if (button == SDL_BUTTON_LEFT) {
        if (g_spawn_mode == 0) {
            // Plant mode
            if (plants_get_type_count() > 0) {
                int new_node = simulation_add_node(world_x, world_y, g_current_plant_type);
                if (new_node >= 0) {
                    PlantType* pt = plants_get_type(g_current_plant_type);
                    printf("Created plant %s at (%.1f, %.1f)\n", pt->name, world_x, world_y);
                }
            }
        } else {
            // Fish mode
            if (fish_get_type_count() > 0) {
                int new_fish = fish_add(world_x, world_y, g_current_fish_type);
                if (new_fish >= 0) {
                    FishType* ft = fish_get_type(g_current_fish_type);
                    printf("Created fish %s at (%.1f, %.1f)\n", ft->name, world_x, world_y);
                }
            } else {
                printf("No fish types available!\n");
            }
        }
    } else if (button == SDL_BUTTON_RIGHT) {
        // Plant chaining (only in plant mode)
        if (g_spawn_mode == 0) {
            int clicked_node = simulation_find_node_at_position(world_x, world_y);
            if (clicked_node >= 0) {
                int selection_mode = simulation_get_selection_mode();
                int selected_node = simulation_get_selected_node();
                
                if (selection_mode == 0) {
                    simulation_set_selected_node(clicked_node);
                    printf("Selected node %d for chaining\n", clicked_node);
                } else if (selection_mode == 1) {
                    if (clicked_node != selected_node) {
                        simulation_add_chain(selected_node, clicked_node);
                        printf("Created chain between nodes %d and %d\n", selected_node, clicked_node);
                    }
                    simulation_clear_selection();
                }
            } else {
                simulation_clear_selection();
            }
        }
    }
}

static void print_debug_info(void) {
    printf("\n=== DEBUG INFO ===\n");
    printf("Plant types: %d\n", plants_get_type_count());
    printf("Fish types: %d\n", fish_get_type_count());
    printf("Total fish: %d\n", fish_get_count());
    printf("Total nodes: %d\n", simulation_get_node_count());
    printf("Spawn mode: %s\n", g_spawn_mode == 0 ? "PLANT" : "FISH");
    printf("Ray rendering: %s\n", fish_is_ray_rendering_enabled() ? "ON" : "OFF");
    
    if (g_spawn_mode == 0 && plants_get_type_count() > 0) {
        PlantType* pt = plants_get_type(g_current_plant_type);
        printf("Current plant: %s\n", pt->name);
    } else if (g_spawn_mode == 1 && fish_get_type_count() > 0) {
        FishType* ft = fish_get_type(g_current_fish_type);
        printf("Current fish: %s\n", ft->name);
    }
    
    // Show fish details
    Fish* fish_list = fish_get_all();
    Node* nodes = simulation_get_nodes();
    for (int i = 0; i < fish_get_count(); i++) {
        if (fish_list[i].active) {
            Node* node = &nodes[fish_list[i].node_id];
            FishType* ft = fish_get_type(fish_list[i].fish_type);
            printf("Fish %d (%s): pos(%.1f,%.1f) node=%d energy=%.2f reward=%.3f\n", 
                   i, ft->name, node->x, node->y, fish_list[i].node_id,
                   fish_list[i].energy, fish_list[i].last_reward);
        }
    }
    printf("==================\n\n");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Starting Great Barrier Reef Ecosystem v2...\n");
    srand((unsigned int)time(NULL));
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    // Create window and renderer
    SDL_Window* window = SDL_CreateWindow("Great Barrier Reef Ecosystem v2",
                                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
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
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // Initialize all systems
    printf("Initializing systems...\n");
    if (!simulation_init()) {
        printf("Simulation init failed\n");
        goto cleanup;
    }
    if (!camera_init()) {
        printf("Camera init failed\n");
        goto cleanup;
    }
    if (!rendering_init(renderer)) {
        printf("Rendering init failed\n");
        goto cleanup;
    }
    if (!nutrition_init()) {
        printf("Nutrition init failed\n");
        goto cleanup;
    }
    if (!gas_init()) {
        printf("Gas init failed\n");
        goto cleanup;
    }
    if (!fish_init()) {
        printf("Fish init failed\n");
        goto cleanup;
    }
    
    // Load configurations
    printf("Loading configurations...\n");
    if (!plants_load_config("plants.conf")) {
        printf("ERROR: Failed to load plants.conf\n");
        goto cleanup;
    }
    
    if (!fish_load_config("fish.conf")) {
        printf("WARNING: Failed to load fish.conf - no fish available\n");
    }
    
    // Initialize Python API
    if (!python_api_init()) {
        printf("WARNING: Python API init failed\n");
    } else {
        if (!python_api_run_script("fish_controller.py")) {
            printf("WARNING: Failed to load fish controller\n");
        }
    }
    
    // Set renderer for layers
    nutrition_set_renderer(renderer);
    gas_set_renderer(renderer);
    
    // Initial setup
    populate_reef_randomly();
    
    // Print status
    printf("\nSystem ready!\n");
    printf("Plant types loaded: %d\n", plants_get_type_count());
    printf("Fish types loaded: %d\n", fish_get_type_count());
    
    // Print controls
    printf("\nControls:\n");
    printf("  Left click: Create organism\n");
    printf("  Right click: Chain plants (plant mode only)\n");
    printf("  WASD: Move camera\n");
    printf("  Shift+WASD: Sprint\n");
    printf("  Mouse wheel: Zoom\n");
    printf("  1-8: Select plant type\n");
    printf("  F1-F6: Select fish type\n");
    printf("  TAB: Toggle plant/fish mode\n");
    printf("  N: Toggle nutrition layer\n");
    printf("  G: Toggle gas layer\n");
    printf("  R: Toggle fish vision rays\n");
    printf("  P: Print debug info\n");
    printf("  ESC: Exit\n\n");
    
    // Set initial mode
    if (plants_get_type_count() > 0) {
        printf("Mode: PLANT (%s)\n", plants_get_type(0)->name);
    }
    
    // Main loop variables
    int running = 1;
    SDL_Event event;
    int mouse_x = 0, mouse_y = 0;
    
    // Main game loop
    while (running) {
        Uint32 frame_start = SDL_GetTicks();
        
        // Get keyboard state
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        int movement_keys[4] = {
            keys[SDL_SCANCODE_W],
            keys[SDL_SCANCODE_A], 
            keys[SDL_SCANCODE_S],
            keys[SDL_SCANCODE_D]
        };
        int sprint = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
        
        // Handle events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                    
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running = 0;
                            break;
                            
                        case SDLK_TAB:
                            g_spawn_mode = !g_spawn_mode;
                            if (g_spawn_mode == 0 && plants_get_type_count() > 0) {
                                printf("Mode: PLANT (%s)\n", plants_get_type(g_current_plant_type)->name);
                            } else if (g_spawn_mode == 1 && fish_get_type_count() > 0) {
                                printf("Mode: FISH (%s)\n", fish_get_type(g_current_fish_type)->name);
                            } else {
                                printf("Mode: %s (no types available)\n", g_spawn_mode == 0 ? "PLANT" : "FISH");
                            }
                            break;
                            
                        case SDLK_n:
                            nutrition_toggle_visibility();
                            break;
                            
                        case SDLK_g:
                            gas_toggle_visibility();
                            break;
                            
                        case SDLK_r:
                            fish_toggle_ray_rendering();
                            break;
                            
                        case SDLK_p:
                            print_debug_info();
                            break;
                            
                        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
                        case SDLK_5: case SDLK_6: case SDLK_7: case SDLK_8: {
                            int plant_index = event.key.keysym.sym - SDLK_1;
                            if (plant_index < plants_get_type_count()) {
                                g_current_plant_type = plant_index;
                                g_spawn_mode = 0;
                                printf("Selected plant: %s\n", plants_get_type(plant_index)->name);
                            }
                            break;
                        }
                        
                        case SDLK_F1: case SDLK_F2: case SDLK_F3:
                        case SDLK_F4: case SDLK_F5: case SDLK_F6: {
                            int fish_index = event.key.keysym.sym - SDLK_F1;
                            if (fish_index < fish_get_type_count()) {
                                g_current_fish_type = fish_index;
                                g_spawn_mode = 1;
                                printf("Selected fish: %s\n", fish_get_type(fish_index)->name);
                            } else {
                                printf("Fish type F%d not available (%d types loaded)\n", 
                                       fish_index + 1, fish_get_type_count());
                            }
                            break;
                        }
                    }
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    handle_mouse_click(event.button.x, event.button.y, event.button.button);
                    break;
                    
                case SDL_MOUSEWHEEL:
                    SDL_GetMouseState(&mouse_x, &mouse_y);
                    camera_zoom(event.wheel.y > 0 ? ZOOM_SPEED : -ZOOM_SPEED, mouse_x, mouse_y);
                    break;
                    
                case SDL_MOUSEMOTION:
                    mouse_x = event.motion.x;
                    mouse_y = event.motion.y;
                    break;
            }
        }
        
        // Update systems
        camera_update_with_sprint(movement_keys, sprint);
        python_api_update();
        fish_update();
        physics_update();
        
        // Render
        rendering_render();
        
        // Frame rate limiting
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }
    
cleanup:
    printf("Shutting down...\n");
    python_api_cleanup();
    fish_cleanup();
    gas_cleanup();
    nutrition_cleanup();
    simulation_cleanup();
    rendering_cleanup();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("Goodbye!\n");
    return 0;
}