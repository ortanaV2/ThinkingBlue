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

static int g_keys[4] = {0};
static int g_current_plant_type = 0;

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
    
    // Initialize systems
    if (!simulation_init() || !camera_init() || !rendering_init(renderer)) {
        printf("System initialization failed\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    int running = 1;
    SDL_Event event;
    int mouse_x = 0, mouse_y = 0;
    
    printf("\nControls:\n");
    printf("  Left click: Create node (current: %s)\n", 
           plants_get_type_count() > 0 ? plants_get_type(g_current_plant_type)->name : "none");
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
                        if (plant_index < plants_get_type_count()) {
                            g_current_plant_type = plant_index;
                            printf("Selected plant type: %s\n", plants_get_type(g_current_plant_type)->name);
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
        
        camera_update(g_keys);
        physics_update();
        rendering_render();
        
        SDL_Delay(16);
    }
    
    // Cleanup systems
    simulation_cleanup();
    rendering_cleanup();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}