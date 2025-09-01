#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define MAX_NODES 500
#define MAX_CHAINS 1000
#define OPTIMAL_DISTANCE 50.0f  // Visual distance (1 unit = 50 pixels)
#define REPULSION_FORCE 0.05f
#define CHAIN_FORCE 0.3f
#define WATER_DRAG 0.95f
#define NODE_RADIUS 5
#define CAMERA_SPEED 5.0f
#define ZOOM_SPEED 0.1f
#define MIN_ZOOM 0.1f
#define MAX_ZOOM 5.0f

typedef struct {
    float x, y;
    float vx, vy; // velocity
    int active;
} Node;

typedef struct {
    int node1, node2;
    int active;
} Chain;

typedef struct {
    float x, y;    // camera position in world coordinates
    float zoom;    // zoom level
} Camera;

typedef struct {
    Node nodes[MAX_NODES];
    Chain chains[MAX_CHAINS];
    int node_count;
    int chain_count;
    int selected_node;
    int selection_mode; // 0 = none, 1 = first node selected
    Camera camera;
    int keys[4]; // WASD states
} Simulation;

Simulation sim = {0};

void init_simulation() {
    sim.node_count = 0;
    sim.chain_count = 0;
    sim.selected_node = -1;
    sim.selection_mode = 0;
    
    // Initialize camera
    sim.camera.x = WINDOW_WIDTH / 2.0f;
    sim.camera.y = WINDOW_HEIGHT / 2.0f;
    sim.camera.zoom = 1.0f;
    
    // Initialize keys
    for (int i = 0; i < 4; i++) {
        sim.keys[i] = 0;
    }
}

// Convert screen coordinates to world coordinates
void screen_to_world(int screen_x, int screen_y, float* world_x, float* world_y) {
    *world_x = (screen_x - WINDOW_WIDTH / 2.0f) / sim.camera.zoom + sim.camera.x;
    *world_y = (screen_y - WINDOW_HEIGHT / 2.0f) / sim.camera.zoom + sim.camera.y;
}

// Convert world coordinates to screen coordinates
void world_to_screen(float world_x, float world_y, int* screen_x, int* screen_y) {
    *screen_x = (int)((world_x - sim.camera.x) * sim.camera.zoom + WINDOW_WIDTH / 2.0f);
    *screen_y = (int)((world_y - sim.camera.y) * sim.camera.zoom + WINDOW_HEIGHT / 2.0f);
}

void update_camera() {
    // WASD movement
    float move_speed = CAMERA_SPEED / sim.camera.zoom; // Adjust speed based on zoom
    
    if (sim.keys[0]) sim.camera.y -= move_speed; // W
    if (sim.keys[1]) sim.camera.x -= move_speed; // A
    if (sim.keys[2]) sim.camera.y += move_speed; // S
    if (sim.keys[3]) sim.camera.x += move_speed; // D
}

void zoom_camera(float zoom_delta, int mouse_x, int mouse_y) {
    // Get world position before zoom
    float world_x_before, world_y_before;
    screen_to_world(mouse_x, mouse_y, &world_x_before, &world_y_before);
    
    // Apply zoom
    sim.camera.zoom *= (1.0f + zoom_delta);
    
    // Clamp zoom
    if (sim.camera.zoom < MIN_ZOOM) sim.camera.zoom = MIN_ZOOM;
    if (sim.camera.zoom > MAX_ZOOM) sim.camera.zoom = MAX_ZOOM;
    
    // Get world position after zoom
    float world_x_after, world_y_after;
    screen_to_world(mouse_x, mouse_y, &world_x_after, &world_y_after);
    
    // Adjust camera position to keep mouse cursor at same world position
    sim.camera.x += world_x_before - world_x_after;
    sim.camera.y += world_y_before - world_y_after;
}

int add_node(float x, float y) {
    if (sim.node_count >= MAX_NODES) return -1;
    
    Node* node = &sim.nodes[sim.node_count];
    node->x = x;
    node->y = y;
    node->vx = 0;
    node->vy = 0;
    node->active = 1;
    
    return sim.node_count++;
}

int add_chain(int node1, int node2) {
    if (sim.chain_count >= MAX_CHAINS) return -1;
    if (node1 == node2) return -1;
    
    // Check if chain already exists
    for (int i = 0; i < sim.chain_count; i++) {
        if (!sim.chains[i].active) continue;
        if ((sim.chains[i].node1 == node1 && sim.chains[i].node2 == node2) ||
            (sim.chains[i].node1 == node2 && sim.chains[i].node2 == node1)) {
            return -1; // Chain already exists
        }
    }
    
    Chain* chain = &sim.chains[sim.chain_count];
    chain->node1 = node1;
    chain->node2 = node2;
    chain->active = 1;
    
    return sim.chain_count++;
}

int find_node_at_position(float world_x, float world_y) {
    for (int i = 0; i < sim.node_count; i++) {
        if (!sim.nodes[i].active) continue;
        
        float dx = sim.nodes[i].x - world_x;
        float dy = sim.nodes[i].y - world_y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance <= NODE_RADIUS * 2 / sim.camera.zoom) { // Adjust hit detection for zoom
            return i;
        }
    }
    return -1;
}

void apply_repulsion_forces() {
    for (int i = 0; i < sim.node_count; i++) {
        if (!sim.nodes[i].active) continue;
        
        for (int j = i + 1; j < sim.node_count; j++) {
            if (!sim.nodes[j].active) continue;
            
            float dx = sim.nodes[j].x - sim.nodes[i].x;
            float dy = sim.nodes[j].y - sim.nodes[i].y;
            float distance = sqrt(dx * dx + dy * dy);
            
            if (distance < OPTIMAL_DISTANCE && distance > 0) {
                float force_magnitude = REPULSION_FORCE * (OPTIMAL_DISTANCE - distance) / distance;
                float fx = -dx * force_magnitude;
                float fy = -dy * force_magnitude;
                
                sim.nodes[i].vx += fx;
                sim.nodes[i].vy += fy;
                sim.nodes[j].vx -= fx;
                sim.nodes[j].vy -= fy;
            }
        }
    }
}

void apply_chain_forces() {
    for (int i = 0; i < sim.chain_count; i++) {
        if (!sim.chains[i].active) continue;
        
        int n1 = sim.chains[i].node1;
        int n2 = sim.chains[i].node2;
        
        if (!sim.nodes[n1].active || !sim.nodes[n2].active) continue;
        
        float dx = sim.nodes[n2].x - sim.nodes[n1].x;
        float dy = sim.nodes[n2].y - sim.nodes[n1].y;
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance > 0) {
            float force_magnitude = CHAIN_FORCE * (distance - OPTIMAL_DISTANCE) / distance;
            float fx = dx * force_magnitude;
            float fy = dy * force_magnitude;
            
            sim.nodes[n1].vx += fx;
            sim.nodes[n1].vy += fy;
            sim.nodes[n2].vx -= fx;
            sim.nodes[n2].vy -= fy;
        }
    }
}

void update_physics() {
    apply_repulsion_forces();
    apply_chain_forces();
    
    // Update positions and apply water drag
    for (int i = 0; i < sim.node_count; i++) {
        if (!sim.nodes[i].active) continue;
        
        // Apply water drag
        sim.nodes[i].vx *= WATER_DRAG;
        sim.nodes[i].vy *= WATER_DRAG;
        
        // Update position
        sim.nodes[i].x += sim.nodes[i].vx;
        sim.nodes[i].y += sim.nodes[i].vy;
        
        // Keep nodes within expanded world bounds (no hard boundaries now)
        // Let nodes move freely in world space
    }
}

void render(SDL_Renderer* renderer) {
    // Clear screen with underwater blue
    SDL_SetRenderDrawColor(renderer, 30, 60, 120, 255);
    SDL_RenderClear(renderer);
    
    // Draw chains
    SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
    for (int i = 0; i < sim.chain_count; i++) {
        if (!sim.chains[i].active) continue;
        
        int n1 = sim.chains[i].node1;
        int n2 = sim.chains[i].node2;
        
        if (!sim.nodes[n1].active || !sim.nodes[n2].active) continue;
        
        int screen_x1, screen_y1, screen_x2, screen_y2;
        world_to_screen(sim.nodes[n1].x, sim.nodes[n1].y, &screen_x1, &screen_y1);
        world_to_screen(sim.nodes[n2].x, sim.nodes[n2].y, &screen_x2, &screen_y2);
        
        SDL_RenderDrawLine(renderer, screen_x1, screen_y1, screen_x2, screen_y2);
    }
    
    // Draw nodes
    for (int i = 0; i < sim.node_count; i++) {
        if (!sim.nodes[i].active) continue;
        
        int screen_x, screen_y;
        world_to_screen(sim.nodes[i].x, sim.nodes[i].y, &screen_x, &screen_y);
        
        // Skip nodes that are outside screen bounds
        int scaled_radius = (int)(NODE_RADIUS * sim.camera.zoom);
        if (screen_x < -scaled_radius || screen_x > WINDOW_WIDTH + scaled_radius ||
            screen_y < -scaled_radius || screen_y > WINDOW_HEIGHT + scaled_radius) {
            continue;
        }
        
        // Highlight selected node
        if (i == sim.selected_node && sim.selection_mode == 1) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 150, 255, 150, 255);
        }
        
        // Draw node as filled circle (approximated with points)
        for (int dx = -scaled_radius; dx <= scaled_radius; dx++) {
            for (int dy = -scaled_radius; dy <= scaled_radius; dy++) {
                if (dx * dx + dy * dy <= scaled_radius * scaled_radius) {
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

void handle_mouse_click(int screen_x, int screen_y, int button) {
    // Convert screen coordinates to world coordinates
    float world_x, world_y;
    screen_to_world(screen_x, screen_y, &world_x, &world_y);
    
    if (button == SDL_BUTTON_LEFT) {
        // Left click: create new node
        add_node(world_x, world_y);
        printf("Created node at world (%.1f, %.1f)\n", world_x, world_y);
    } else if (button == SDL_BUTTON_RIGHT) {
        // Right click: chain selection
        int clicked_node = find_node_at_position(world_x, world_y);
        
        if (clicked_node >= 0) {
            if (sim.selection_mode == 0) {
                // First node selection
                sim.selected_node = clicked_node;
                sim.selection_mode = 1;
                printf("Selected node %d for chaining\n", clicked_node);
            } else if (sim.selection_mode == 1) {
                // Second node selection - create chain
                if (clicked_node != sim.selected_node) {
                    int chain_id = add_chain(sim.selected_node, clicked_node);
                    if (chain_id >= 0) {
                        printf("Created chain between nodes %d and %d\n", sim.selected_node, clicked_node);
                    }
                }
                sim.selection_mode = 0;
                sim.selected_node = -1;
            }
        } else {
            // Clicked on empty space - cancel selection
            sim.selection_mode = 0;
            sim.selected_node = -1;
            printf("Selection cancelled\n");
        }
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow("Organism Simulation",
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
    
    // Main loop
    int running = 1;
    SDL_Event event;
    int mouse_x = 0, mouse_y = 0;
    
    printf("Controls:\n");
    printf("Left click: Create node\n");
    printf("Right click: Select nodes for chaining\n");
    printf("WASD: Move camera\n");
    printf("Mouse wheel: Zoom in/out\n");
    printf("ESC: Exit\n");
    
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
                    // WASD controls
                    else if (event.key.keysym.sym == SDLK_w) sim.keys[0] = 1;
                    else if (event.key.keysym.sym == SDLK_a) sim.keys[1] = 1;
                    else if (event.key.keysym.sym == SDLK_s) sim.keys[2] = 1;
                    else if (event.key.keysym.sym == SDLK_d) sim.keys[3] = 1;
                    break;
                    
                case SDL_KEYUP:
                    // WASD controls
                    if (event.key.keysym.sym == SDLK_w) sim.keys[0] = 0;
                    else if (event.key.keysym.sym == SDLK_a) sim.keys[1] = 0;
                    else if (event.key.keysym.sym == SDLK_s) sim.keys[2] = 0;
                    else if (event.key.keysym.sym == SDLK_d) sim.keys[3] = 0;
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
        
        SDL_Delay(16); // ~60 FPS
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}