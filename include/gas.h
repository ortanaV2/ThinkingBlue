#ifndef GAS_H
#define GAS_H

#include <SDL2/SDL.h>

// Gas layer system
int gas_init(void);
void gas_cleanup(void);
void gas_set_renderer(SDL_Renderer* renderer);

// Toggle visibility
void gas_toggle_visibility(void);
int gas_is_visible(void);

// Render gas layer
void gas_render(void);

// Get oxygen value at world position
float gas_get_oxygen_at(float world_x, float world_y);

// Oxygen production and decay
void gas_produce_oxygen_at_position(float world_x, float world_y, float production_amount, float radius);
void gas_decay_oxygen(void);

#endif // GAS_H