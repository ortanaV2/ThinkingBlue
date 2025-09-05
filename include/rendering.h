#ifndef RENDERING_H
#define RENDERING_H

#include <SDL2/SDL.h>

// Rendering system initialization and cleanup
int rendering_init(SDL_Renderer* renderer);
void rendering_cleanup(void);

// Main rendering function
void rendering_render(void);

#endif // RENDERING_H