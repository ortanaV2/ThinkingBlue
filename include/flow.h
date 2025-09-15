#ifndef FLOW_H
#define FLOW_H

#include <SDL2/SDL.h>

// Flow field system
int flow_init(void);
void flow_cleanup(void);
void flow_set_renderer(SDL_Renderer* renderer);

// Toggle visibility
void flow_toggle_visibility(void);
int flow_is_visible(void);

// Render flow field
void flow_render(void);

// Get flow vector at world position
void flow_get_vector_at(float world_x, float world_y, float* flow_x, float* flow_y);

// Get flow magnitude at position
float flow_get_magnitude_at(float world_x, float world_y);

#endif // FLOW_H