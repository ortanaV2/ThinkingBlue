#ifndef CAMERA_H
#define CAMERA_H

// Camera system initialization
int camera_init(void);

// Camera movement and control
void camera_update(int keys[4]);
void camera_update_with_sprint(int keys[4], int sprint_active);
void camera_zoom(float zoom_delta, int mouse_x, int mouse_y);

// Coordinate transformations
void camera_screen_to_world(int screen_x, int screen_y, float* world_x, float* world_y);
void camera_world_to_screen(float world_x, float world_y, int* screen_x, int* screen_y);

// Camera state accessors
float camera_get_zoom(void);
void camera_get_viewport_bounds(float* left, float* top, float* right, float* bottom);

#endif // CAMERA_H