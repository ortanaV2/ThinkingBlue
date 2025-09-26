// main.c - Enhanced with fixed fish counting for statistics
#include <SDL2/SDL.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

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
#include "flow.h"
#include "temperature.h"

#define FRAME_DELAY (1000 / TARGET_FPS)

// Global state
static int g_current_plant_type = 0;
static int g_current_fish_type = 0;
static int g_spawn_mode = 0; // 0 = plants, 1 = fish
static int g_graceful_shutdown_requested = 0;

// FPS tracking variables
static Uint32 g_frame_count = 0;
static Uint32 g_fps_start_time = 0;

// Signal handler for graceful shutdown
static void signal_handler(int signum) {
    printf("\nReceived signal %d, initiating graceful shutdown for model saving...\n", signum);
    g_graceful_shutdown_requested = 1;
    
    // Clean up stats file immediately
    remove("simulation_stats.tmp");
    printf("Cleaned up simulation_stats.tmp\n");
}

// Calculate and update FPS
static void update_fps(void) {
    g_frame_count++;
    Uint32 current_time = SDL_GetTicks();
    
    if (g_fps_start_time == 0) {
        g_fps_start_time = current_time;
    }
    
    Uint32 elapsed = current_time - g_fps_start_time;
    if (elapsed >= 1000) { // Update every second
        float fps = (float)g_frame_count * 1000.0f / (float)elapsed;
        rendering_update_fps(fps);
        
        g_frame_count = 0;
        g_fps_start_time = current_time;
    }
}

static void populate_reef_randomly(void) {
    int total_plant_species = plants_get_type_count();
    int total_fish_species = fish_get_type_count();
    
    if (total_plant_species == 0) return;
    
    printf("Populating reef with %d plants and %d fish...\n", 
           INITIAL_PLANT_COUNT, INITIAL_FISH_COUNT);
    
    // Spawn plants
    for (int i = 0; i < INITIAL_PLANT_COUNT; i++) {
        float x = WORLD_LEFT + ((float)rand() / RAND_MAX) * WORLD_WIDTH;
        float y = WORLD_TOP + ((float)rand() / RAND_MAX) * WORLD_HEIGHT;
        int species = rand() % total_plant_species;
        simulation_add_node(x, y, species);
    }
    
    // Spawn fish
    if (total_fish_species > 0) {
        printf("Spawning %d fish with neural networks...\n", INITIAL_FISH_COUNT);
        
        for (int i = 0; i < INITIAL_FISH_COUNT; i++) {
            float x = WORLD_LEFT + ((float)rand() / RAND_MAX) * WORLD_WIDTH;
            float y = WORLD_TOP + ((float)rand() / RAND_MAX) * WORLD_HEIGHT;
            int fish_type = rand() % total_fish_species;
            
            int fish_id = fish_add(x, y, fish_type);
            if (fish_id >= 0) {
                FishType* ft = fish_get_type(fish_type);
                float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
                printf("Spawned %s at (%.0f, %.0f) - max age: %.1f min\n", 
                       ft->name, x, y, lifespan_minutes);
            }
        }
    }
    
    printf("Reef populated!\n");
}

// Count only currently active fish
static int count_active_fish(void) {
    Fish* all_fish = fish_get_all();
    int total_fish = fish_get_count();
    int active_count = 0;
    
    for (int i = 0; i < total_fish; i++) {
        if (all_fish[i].active) {
            active_count++;
        }
    }
    
    return active_count;
}

static void write_stats_file(void) {
    static int last_write_frame = 0;
    int current_frame = simulation_get_frame_counter();
    
    // Write stats every 30 frames (1 second at 30 FPS)
    if (current_frame - last_write_frame < 30) {
        return;
    }
    last_write_frame = current_frame;
    
    // Check for temperature control from Python GUI
    FILE* temp_control = fopen("temperature_control.tmp", "r");
    if (temp_control) {
        char line[32];
        if (fgets(line, sizeof(line), temp_control)) {
            float new_temp = atof(line);
            temperature_set_current(new_temp);
        }
        fclose(temp_control);
    }
    
    FILE* stats_file = fopen("simulation_stats.tmp", "wb");
    if (!stats_file) {
        return;
    }
    
    // Get current statistics
    int fish_count = count_active_fish();
    int plant_count = 0;
    
    // Count active plant nodes
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active && nodes[i].plant_type >= 0) {
            plant_count++;
        }
    }
    
    // Get nutrition data
    float total_environmental_nutrition = plants_get_total_environmental_nutrition();
    
    // Get temperature and bleached count
    float current_temperature = temperature_get_current();
    int bleached_count = 0;
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active && temperature_is_coral_bleached(i)) {
            bleached_count++;
        }
    }
    
    // Write binary data: total_nutrition, fish_count, plant_count, temperature, bleached_count
    float data[5] = {total_environmental_nutrition, (float)fish_count, (float)plant_count, 
                     current_temperature, (float)bleached_count};
    fwrite(data, sizeof(float), 5, stats_file);
    
    fclose(stats_file);
}

static void start_stats_plotter(void) {
    printf("Starting ecosystem statistics monitor with temperature control...\n");
    
    // Execute ecosystem stats in background
    #ifdef _WIN32
        system("start python ecosystem_stats.py");
    #else
        system("python3 ecosystem_stats.py &");
    #endif
    
    printf("Statistics monitor started! Live plots should appear with temperature slider.\n");
    printf("Data is updated every second via simulation_stats.tmp\n");
    printf("You can close and reopen the statistics window anytime with TAB\n");
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
                    float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
                    printf("Created fish %s at (%.1f, %.1f) - max age: %.1f min\n", 
                           ft->name, world_x, world_y, lifespan_minutes);
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
    printf("World size: %.0fx%.0f\n", WORLD_WIDTH, WORLD_HEIGHT);
    printf("Zoom: unlimited (current: %.6f)\n", camera_get_zoom());
    printf("Plant types: %d\n", plants_get_type_count());
    printf("Fish types: %d\n", fish_get_type_count());
    printf("Active fish: %d\n", count_active_fish());
    printf("Total nodes: %d\n", simulation_get_node_count());
    printf("Spawn mode: %s\n", g_spawn_mode == 0 ? "PLANT" : "FISH");
    printf("Ray rendering: %s\n", fish_is_ray_rendering_enabled() ? "ON" : "OFF");
    printf("Flow field: %s\n", flow_is_visible() ? "ON" : "OFF");
    printf("Temperature: %.1f°C\n", temperature_get_current());
    printf("Standard depletion range: %.1f\n", STANDARD_DEPLETION_RANGE);
    printf("Statistics: Available via TAB key\n");
    printf("FPS display: Enabled in top-right corner\n");
    
    // Count bleached corals
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    int bleached_count = 0;
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active && temperature_is_coral_bleached(i)) {
            bleached_count++;
        }
    }
    printf("Bleached corals: %d\n", bleached_count);
    
    // Aging system statistics
    printf("\n=== AGING SYSTEM STATUS ===\n");
    printf("Total deaths from aging: %d\n", fish_get_total_deaths_from_age());
    printf("Death check interval: %d frames (%.1f sec)\n", DEATH_CHECK_INTERVAL, DEATH_CHECK_INTERVAL / (float)TARGET_FPS);
    
    // Fish age distribution
    Fish* fish_list = fish_get_all();
    int total_fish = fish_get_count();
    int current_frame = simulation_get_frame_counter();
    int young_fish = 0, middle_aged_fish = 0, old_fish = 0;
    
    for (int i = 0; i < total_fish; i++) {
        if (fish_list[i].active) {
            FishType* ft = fish_get_type(fish_list[i].fish_type);
            if (ft) {
                int age = current_frame - fish_list[i].birth_frame;
                float age_ratio = (float)age / (float)ft->max_age;
                
                if (age_ratio < 0.33f) young_fish++;
                else if (age_ratio < 0.66f) middle_aged_fish++;
                else old_fish++;
            }
        }
    }
    
    printf("Age distribution: %d young, %d middle-aged, %d old\n", young_fish, middle_aged_fish, old_fish);
    
    // Nutrition cycle balance
    printf("\n=== NUTRITION SYSTEM ===\n");
    float fish_consumed = fish_get_total_nutrition_consumed();
    float fish_defecated = fish_get_total_nutrition_defecated();
    float total_environmental = plants_get_total_environmental_nutrition();
    
    printf("Fish consumed: %.4f\n", fish_consumed);
    printf("Fish defecated: %.4f\n", fish_defecated);
    printf("Fish balance: %.4f (should be close to 0 when balanced)\n", fish_consumed - fish_defecated);
    printf("Total environmental nutrition: %.4f\n", total_environmental);
    printf("Defecation threshold: 70%% stomach full\n");
    printf("Defecation empties: 100%% of stomach\n");
    printf("Range used: %.1f (same for depletion and defecation)\n", STANDARD_DEPLETION_RANGE);
    
    if (g_spawn_mode == 0 && plants_get_type_count() > 0) {
        PlantType* pt = plants_get_type(g_current_plant_type);
        printf("Current plant: %s\n", pt->name);
    } else if (g_spawn_mode == 1 && fish_get_type_count() > 0) {
        FishType* ft = fish_get_type(g_current_fish_type);
        float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
        printf("Current fish: %s (max age: %.1f min)\n", ft->name, lifespan_minutes);
    }
    
    printf("\n=== NEURAL NETWORK STATUS ===\n");
    printf("Models will be saved on exit (Ctrl+C or ESC)\n");
    printf("Best herbivore and predator models will be saved to JSON files\n");
    printf("Training is ongoing - reproduction success tracked for model selection\n");
    printf("==========================================\n\n");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Starting Great Barrier Reef Ecosystem v3...\n");
    printf("World dimensions: %.0fx%.0f, Initial population: %d plants, %d fish\n",
           WORLD_WIDTH, WORLD_HEIGHT, INITIAL_PLANT_COUNT, INITIAL_FISH_COUNT);
    printf("Temperature system active - coral bleaching will occur at temperatures > 0°C\n");
    printf("FPS display enabled in top-right corner\n");
    printf("Best models will be saved on graceful shutdown (Ctrl+C)\n");
    printf("Live statistics plotter available with temperature control (press 'TAB')\n");
    
    srand((unsigned int)time(NULL));
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // Termination request
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    // Create window and renderer
    SDL_Window* window = SDL_CreateWindow("ThinkingBlue Ecosystem Simulation",
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
    if (!flow_init()) {
        printf("Flow init failed\n");
        goto cleanup;
    }
    if (!fish_init()) {
        printf("Fish init failed\n");
        goto cleanup;
    }
    if (!temperature_init()) {
        printf("Temperature init failed\n");
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
    
    // Initialize Python API with error checking
    if (!python_api_init()) {
        printf("CRITICAL ERROR: Python API init failed - Python installation broken\n");
        printf("Please fix Python installation before running simulation\n");
        return 1;
    }
    
    // Try to load Python script with error handling
    if (!python_api_run_script("fish_controller.py")) {
        printf("CRITICAL ERROR: Failed to load fish controller - Neural networks disabled\n");
        printf("Python environment is not working correctly\n");
        printf("Please fix Python installation (missing math module)\n");
        return 1;
    }
    
    printf("Python neural network controller loaded successfully!\n");
    
    // Set renderer for layers
    nutrition_set_renderer(renderer);
    gas_set_renderer(renderer);
    flow_set_renderer(renderer);
    
    // Initial setup
    populate_reef_randomly();
    
    // Print status
    printf("\nSystem ready!\n");
    printf("Plant types loaded: %d\n", plants_get_type_count());
    printf("Fish types loaded: %d\n", fish_get_type_count());
    printf("Temperature: %.1f°C (use stats GUI to adjust)\n", temperature_get_current());
    printf("Standard nutrition depletion range: %.1f\n", STANDARD_DEPLETION_RANGE);
    
    // Print controls
    printf("\nControls:\n");
    printf("  Left click: Create organism\n");
    printf("  Right click: Chain plants (plant mode only)\n");
    printf("  WASD: Move camera\n");
    printf("  Shift+WASD: Sprint\n");
    printf("  Mouse wheel: Zoom (unlimited range)\n");
    printf("  1-8: Select plant type\n");
    printf("  F1-F6: Select fish type\n");
    printf("  TAB: Open statistics plotter with temperature control\n");
    printf("  Shift+TAB: Toggle plant/fish mode\n");
    printf("  N: Toggle nutrition layer\n");
    printf("  G: Toggle gas layer\n");
    printf("  F: Toggle flow field\n");
    printf("  R: Toggle fish vision rays\n");
    printf("  P: Print debug info\n");
    printf("  ESC or Ctrl+C: Save best models and exit (cleans temp files)\n\n");
    
    // Set initial mode
    if (plants_get_type_count() > 0) {
        printf("Mode: PLANT (%s)\n", plants_get_type(0)->name);
    }
    
    // Main loop variables
    int running = 1;
    SDL_Event event;
    int mouse_x = 0, mouse_y = 0;
    
    printf("Use the statistics GUI (TAB) to monitor live population and temperature.\n\n");
    
    // Main game loop
    while (running && !g_graceful_shutdown_requested) {
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
                            // Check if Shift is held for spawn mode toggle
                            const Uint8* keystate = SDL_GetKeyboardState(NULL);
                            if (keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT]) {
                                // Shift+Tab: Toggle spawn mode
                                g_spawn_mode = !g_spawn_mode;
                                if (g_spawn_mode == 0 && plants_get_type_count() > 0) {
                                    printf("Mode: PLANT (%s)\n", plants_get_type(g_current_plant_type)->name);
                                } else if (g_spawn_mode == 1 && fish_get_type_count() > 0) {
                                    FishType* ft = fish_get_type(g_current_fish_type);
                                    float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
                                    printf("Mode: FISH (%s, max age: %.1f min)\n", ft->name, lifespan_minutes);
                                } else {
                                    printf("Mode: %s (no types available)\n", g_spawn_mode == 0 ? "PLANT" : "FISH");
                                }
                            } else {
                                // Tab: Open statistics
                                start_stats_plotter();
                            }
                            break;
                            
                        case SDLK_n:
                            nutrition_toggle_visibility();
                            break;
                            
                        case SDLK_g:
                            gas_toggle_visibility();
                            break;
                            
                        case SDLK_f:
                            flow_toggle_visibility();
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
                                FishType* ft = fish_get_type(fish_index);
                                float lifespan_minutes = ft->max_age / (TARGET_FPS * 60.0f);
                                printf("Selected fish: %s (max age: %.1f min)\n", ft->name, lifespan_minutes);
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
        temperature_process_coral_bleaching();
        
        // Update FPS calculation
        update_fps();
        
        // Write statistics for plotter
        write_stats_file();
        
        // Render everything
        rendering_render();
        
        // Frame rate limiting
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }
    
cleanup:
    printf("Shutting down and saving neural network models...\n");
    
    // Clean up stats file
    remove("simulation_stats.tmp");
    
    // Give Python time to save models if graceful shutdown was requested
    if (g_graceful_shutdown_requested) {
        printf("Allowing Python to save best models...\n");
    }
    
    // Final report
    printf("\n=== FINAL TRAINING REPORT ===\n");
    printf("Total deaths from aging: %d\n", fish_get_total_deaths_from_age());
    printf("Final temperature: %.1f°C\n", temperature_get_current());
    printf("Final active fish count: %d\n", count_active_fish());
    
    // Count final bleached corals
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    int bleached_count = 0;
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active && temperature_is_coral_bleached(i)) {
            bleached_count++;
        }
    }
    printf("Total bleached corals: %d\n", bleached_count);
    
    printf("Fish consumed: %.2f\n", fish_get_total_nutrition_consumed());
    printf("Fish defecated: %.2f\n", fish_get_total_nutrition_defecated());
    printf("Fish balance: %.2f\n", fish_get_nutrition_balance());
    printf("Total environmental nutrition: %.2f\n", plants_get_total_environmental_nutrition());
    printf("Neural network training completed successfully\n");
    printf("Check for best_herbivore_model.json and best_predator_model.json files\n");
    printf("========================================\n");
    
    temperature_cleanup();
    python_api_cleanup();
    fish_cleanup();
    flow_cleanup();
    gas_cleanup();
    nutrition_cleanup();
    simulation_cleanup();
    rendering_cleanup();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("Training session complete! Models saved for future use.\n");
    return 0;
}