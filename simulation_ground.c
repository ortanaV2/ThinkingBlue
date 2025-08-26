#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define MAX_ALGAE 25
#define MAX_SEGMENTS_PER_ALGAE 300
#define CONNECTION_THRESHOLD 12.0f

typedef struct {
    float x, y;
    float angle;
    float length;
    float thickness;
    int parent_index;
    int age;
} Segment;

typedef struct {
    float center_x, center_y;
    Segment segments[MAX_SEGMENTS_PER_ALGAE];
    int segment_count;
    float chaos_factor; // Controls growth randomness
    float density; // Growth frequency
    Uint8 r, g, b;
    float maturity;
    int active;
    int connected_to[MAX_ALGAE];
    int connection_count;
} Algae;

typedef struct {
    Algae algae[MAX_ALGAE];
    int algae_count;
    SDL_Renderer* renderer;
    int running;
} Simulation;

float distance(float x1, float y1, float x2, float y2) {
    return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

void init_algae(Algae* alga) {
    alga->center_x = (float)(rand() % (WINDOW_WIDTH - 300) + 150);
    alga->center_y = (float)(rand() % (WINDOW_HEIGHT - 300) + 150);
    alga->segment_count = 0;
    alga->chaos_factor = 0.3f + (rand() % 100) / 100.0f;
    alga->density = 0.5f + (rand() % 100) / 200.0f;
    alga->maturity = 0.0f;
    alga->active = 1;
    alga->connection_count = 0;
    
    // Random green color variations
    int color_type = rand() % 6;
    switch(color_type) {
        case 0: // Dark green
            alga->r = 15 + rand() % 40;
            alga->g = 60 + rand() % 60;
            alga->b = 25 + rand() % 30;
            break;
        case 1: // Light green
            alga->r = 50 + rand() % 40;
            alga->g = 100 + rand() % 80;
            alga->b = 30 + rand() % 40;
            break;
        case 2: // Yellow-green
            alga->r = 70 + rand() % 50;
            alga->g = 130 + rand() % 60;
            alga->b = 15 + rand() % 30;
            break;
        case 3: // Blue-green
            alga->r = 10 + rand() % 30;
            alga->g = 80 + rand() % 50;
            alga->b = 60 + rand() % 40;
            break;
        case 4: // Olive green
            alga->r = 60 + rand() % 40;
            alga->g = 80 + rand() % 50;
            alga->b = 20 + rand() % 25;
            break;
        default: // Dark blue-green
            alga->r = 20 + rand() % 25;
            alga->g = 70 + rand() % 40;
            alga->b = 50 + rand() % 35;
            break;
    }
    
    for (int i = 0; i < MAX_ALGAE; i++) {
        alga->connected_to[i] = -1;
    }
    
    // Start with 1-2 random segments from center
    int initial_count = 1 + rand() % 2;
    for (int i = 0; i < initial_count; i++) {
        alga->segments[alga->segment_count].x = alga->center_x;
        alga->segments[alga->segment_count].y = alga->center_y;
        alga->segments[alga->segment_count].angle = (rand() % 360) * M_PI / 180.0f;
        alga->segments[alga->segment_count].length = 8.0f + (rand() % 15);
        alga->segments[alga->segment_count].thickness = 2.5f + (rand() % 4);
        alga->segments[alga->segment_count].parent_index = -1;
        alga->segments[alga->segment_count].age = 0;
        alga->segment_count++;
    }
}

void add_segment(Algae* alga, int parent_index) {
    if (alga->segment_count >= MAX_SEGMENTS_PER_ALGAE) return;
    
    Segment* parent = &alga->segments[parent_index];
    Segment* new_segment = &alga->segments[alga->segment_count];
    
    float parent_end_x = parent->x + cos(parent->angle) * parent->length;
    float parent_end_y = parent->y + sin(parent->angle) * parent->length;
    
    // Don't grow too far from center
    float dist_from_center = distance(parent_end_x, parent_end_y, alga->center_x, alga->center_y);
    if (dist_from_center > 120.0f + rand() % 80) return;
    
    new_segment->x = parent_end_x;
    new_segment->y = parent_end_y;
    new_segment->parent_index = parent_index;
    new_segment->age = 0;
    
    // High chaos in angle - up to ±70 degrees
    float chaos_angle = (rand() % 140 - 70) * alga->chaos_factor * M_PI / 180.0f;
    new_segment->angle = parent->angle + chaos_angle;
    
    // Random length with high variation
    new_segment->length = 5.0f + (rand() % 20);
    
    // Chaotic thickness changes
    float thickness_chaos = 0.6f + (rand() % 80) / 100.0f;
    new_segment->thickness = parent->thickness * thickness_chaos;
    if (new_segment->thickness > 6.0f) new_segment->thickness = 6.0f;
    if (new_segment->thickness < 0.5f) new_segment->thickness = 0.5f;
    
    alga->segment_count++;
}

void check_connections(Simulation* sim, int alga_index) {
    Algae* alga = &sim->algae[alga_index];
    
    for (int i = 0; i < sim->algae_count; i++) {
        if (i == alga_index) continue;
        
        Algae* other = &sim->algae[i];
        if (!other->active) continue;
        
        // Check if already connected
        int already_connected = 0;
        for (int c = 0; c < alga->connection_count; c++) {
            if (alga->connected_to[c] == i) {
                already_connected = 1;
                break;
            }
        }
        if (already_connected) continue;
        
        // Find closest segments
        float min_dist = CONNECTION_THRESHOLD + 1;
        
        for (int s1 = 0; s1 < alga->segment_count; s1++) {
            float seg1_end_x = alga->segments[s1].x + cos(alga->segments[s1].angle) * alga->segments[s1].length;
            float seg1_end_y = alga->segments[s1].y + sin(alga->segments[s1].angle) * alga->segments[s1].length;
            
            for (int s2 = 0; s2 < other->segment_count; s2++) {
                float seg2_end_x = other->segments[s2].x + cos(other->segments[s2].angle) * other->segments[s2].length;
                float seg2_end_y = other->segments[s2].y + sin(other->segments[s2].angle) * other->segments[s2].length;
                
                float dist = distance(seg1_end_x, seg1_end_y, seg2_end_x, seg2_end_y);
                
                if (dist < min_dist) {
                    min_dist = dist;
                }
            }
        }
        
        // Create connection if close enough
        if (min_dist <= CONNECTION_THRESHOLD && alga->connection_count < MAX_ALGAE) {
            alga->connected_to[alga->connection_count] = i;
            alga->connection_count++;
            
            if (other->connection_count < MAX_ALGAE) {
                other->connected_to[other->connection_count] = alga_index;
                other->connection_count++;
            }
        }
    }
}

void update_algae(Algae* alga) {
    if (!alga->active) return;
    
    alga->maturity += 0.0005f;
    
    // Age all segments
    for (int i = 0; i < alga->segment_count; i++) {
        alga->segments[i].age++;
    }
    
    // Multiple chaotic growth attempts per update
    int growth_attempts = 1 + (rand() % 4);
    
    for (int attempt = 0; attempt < growth_attempts; attempt++) {
        float growth_chance = 0.15f + alga->density * 0.3f - alga->maturity * 0.8f;
        if (growth_chance < 0.05f) growth_chance = 0.05f;
        
        if ((rand() % 1000) / 1000.0f < growth_chance) {
            // Pick completely random segment
            if (alga->segment_count > 0) {
                int growth_point = rand() % alga->segment_count;
                
                // Prefer younger segments but not exclusively
                if (rand() % 100 < 60) {
                    int young_segments[MAX_SEGMENTS_PER_ALGAE];
                    int young_count = 0;
                    int max_age_threshold = 50 + rand() % 100;
                    
                    for (int i = 0; i < alga->segment_count; i++) {
                        if (alga->segments[i].age < max_age_threshold) {
                            young_segments[young_count] = i;
                            young_count++;
                        }
                    }
                    
                    if (young_count > 0) {
                        growth_point = young_segments[rand() % young_count];
                    }
                }
                
                add_segment(alga, growth_point);
            }
        }
        
        // Spontaneous new branches from center
        if ((rand() % 2000) == 0) {
            if (alga->segment_count < MAX_SEGMENTS_PER_ALGAE) {
                alga->segments[alga->segment_count].x = alga->center_x + (rand() % 10 - 5);
                alga->segments[alga->segment_count].y = alga->center_y + (rand() % 10 - 5);
                alga->segments[alga->segment_count].angle = (rand() % 360) * M_PI / 180.0f;
                alga->segments[alga->segment_count].length = 6.0f + (rand() % 18);
                alga->segments[alga->segment_count].thickness = 1.5f + (rand() % 4);
                alga->segments[alga->segment_count].parent_index = -1;
                alga->segments[alga->segment_count].age = 0;
                alga->segment_count++;
            }
        }
    }
}

void draw_connections(SDL_Renderer* renderer, Simulation* sim) {
    for (int i = 0; i < sim->algae_count; i++) {
        Algae* alga = &sim->algae[i];
        if (!alga->active) continue;
        
        for (int c = 0; c < alga->connection_count; c++) {
            int other_index = alga->connected_to[c];
            if (other_index < 0 || other_index >= sim->algae_count) continue;
            if (other_index <= i) continue; // Avoid duplicate lines
            
            Algae* other = &sim->algae[other_index];
            if (!other->active) continue;
            
            // Find closest points
            float min_dist = 1000.0f;
            float best_x1, best_y1, best_x2, best_y2;
            
            for (int s1 = 0; s1 < alga->segment_count; s1++) {
                float x1 = alga->segments[s1].x + cos(alga->segments[s1].angle) * alga->segments[s1].length;
                float y1 = alga->segments[s1].y + sin(alga->segments[s1].angle) * alga->segments[s1].length;
                
                for (int s2 = 0; s2 < other->segment_count; s2++) {
                    float x2 = other->segments[s2].x + cos(other->segments[s2].angle) * other->segments[s2].length;
                    float y2 = other->segments[s2].y + sin(other->segments[s2].angle) * other->segments[s2].length;
                    
                    float dist = distance(x1, y1, x2, y2);
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_x1 = x1; best_y1 = y1;
                        best_x2 = x2; best_y2 = y2;
                    }
                }
            }
            
            SDL_SetRenderDrawColor(renderer, 
                                 (alga->r + other->r) / 3, 
                                 (alga->g + other->g) / 3, 
                                 (alga->b + other->b) / 3, 255);
            SDL_RenderDrawLine(renderer, (int)best_x1, (int)best_y1, (int)best_x2, (int)best_y2);
        }
    }
}

void draw_algae(SDL_Renderer* renderer, Algae* alga) {
    if (!alga->active) return;
    
    SDL_SetRenderDrawColor(renderer, alga->r, alga->g, alga->b, 255);
    
    for (int i = 0; i < alga->segment_count; i++) {
        Segment* segment = &alga->segments[i];
        
        float end_x = segment->x + cos(segment->angle) * segment->length;
        float end_y = segment->y + sin(segment->angle) * segment->length;
        
        // Variable thickness with slight organic variation
        int thickness = (int)(segment->thickness * (1.0f + sin(segment->age * 0.1f) * 0.1f));
        if (thickness < 1) thickness = 1;
        
        // Draw thick lines by drawing multiple offset lines
        for (int t = -thickness/2; t <= thickness/2; t++) {
            for (int s = -thickness/2; s <= thickness/2; s++) {
                SDL_RenderDrawLine(renderer, 
                                 (int)(segment->x + t), (int)(segment->y + s), 
                                 (int)(end_x + t), (int)(end_y + s));
            }
        }
        
        // Add organic details randomly
        if (thickness > 2 && (rand() % 8) == 0) {
            SDL_SetRenderDrawColor(renderer, alga->r + 15, alga->g + 15, alga->b + 10, 255);
            SDL_RenderDrawPoint(renderer, (int)end_x + (rand() % 3 - 1), (int)end_y + (rand() % 3 - 1));
            SDL_SetRenderDrawColor(renderer, alga->r, alga->g, alga->b, 255);
        }
    }
}

void update_simulation(Simulation* sim) {
    for (int i = 0; i < sim->algae_count; i++) {
        update_algae(&sim->algae[i]);
        check_connections(sim, i);
    }
    
    // Add new algae randomly
    if (sim->algae_count < MAX_ALGAE && (rand() % 1500) == 0) {
        init_algae(&sim->algae[sim->algae_count]);
        sim->algae_count++;
    }
}

void render_simulation(Simulation* sim) {
    SDL_SetRenderDrawColor(sim->renderer, 0, 0, 0, 255);
    SDL_RenderClear(sim->renderer);
    
    draw_connections(sim->renderer, sim);
    
    for (int i = 0; i < sim->algae_count; i++) {
        draw_algae(sim->renderer, &sim->algae[i]);
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
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_SPACE) {
                    if (sim->algae_count < MAX_ALGAE) {
                        init_algae(&sim->algae[sim->algae_count]);
                        sim->algae_count++;
                    }
                } else if (event.key.keysym.sym == SDLK_r) {
                    sim->algae_count = 0;
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    sim->running = 0;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (sim->algae_count < MAX_ALGAE) {
                        Algae* new_alga = &sim->algae[sim->algae_count];
                        init_algae(new_alga);
                        new_alga->center_x = (float)event.button.x;
                        new_alga->center_y = (float)event.button.y;
                        // Update first segment position
                        if (new_alga->segment_count > 0) {
                            new_alga->segments[0].x = new_alga->center_x;
                            new_alga->segments[0].y = new_alga->center_y;
                        }
                        sim->algae_count++;
                    }
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
    
    SDL_Window* window = SDL_CreateWindow("Chaotic Algae Growth Simulation",
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
    sim.algae_count = 0;
    
    // Start with 2-3 algae
    for (int i = 0; i < 2 + rand() % 2; i++) {
        init_algae(&sim.algae[sim.algae_count]);
        sim.algae_count++;
    }
    
    printf("Chaotic Algae Growth Simulation started!\n");
    printf("Controls:\n");
    printf("- SPACE: Add new algae\n");
    printf("- Left click: Plant algae at mouse position\n");
    printf("- R: Reset simulation\n");
    printf("- ESC: Exit\n");
    
    while (sim.running) {
        handle_events(&sim);
        update_simulation(&sim);
        render_simulation(&sim);
        
        SDL_Delay(60);
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}