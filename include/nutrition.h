#ifndef NUTRITION_H
#define NUTRITION_H

#include <SDL2/SDL.h>

// Nutrition floor system
int nutrition_init(void);
void nutrition_cleanup(void);
void nutrition_set_renderer(SDL_Renderer* renderer);

// Toggle visibility
void nutrition_toggle_visibility(void);
int nutrition_is_visible(void);

// Render nutrition floor
void nutrition_render(void);

// Get nutrition value at world position
float nutrition_get_value_at(float world_x, float world_y);

// Nutrition depletion and regeneration
void nutrition_deplete_at_position(float world_x, float world_y, float depletion_amount, float radius);
void nutrition_add_at_position(float world_x, float world_y, float addition_amount, float radius);
void nutrition_regenerate(void);

// Nutrition balance tracking
float nutrition_get_total_added(void);
float nutrition_get_total_depleted(void);
float nutrition_get_balance(void);

#endif // NUTRITION_H