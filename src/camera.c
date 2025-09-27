#include "types.h"
#include "camera.h"

static Camera g_camera = {0};

int camera_init(void) {
    // Initialize camera at world center
    g_camera.x = WORLD_CENTER_X;
    g_camera.y = WORLD_CENTER_Y;
    g_camera.zoom = 1.0f;
    return 1;
}

void camera_update(int keys[4]) {
    float move_speed = CAMERA_SPEED / g_camera.zoom;
    
    if (keys[0]) g_camera.y -= move_speed; // W
    if (keys[1]) g_camera.x -= move_speed; // A
    if (keys[2]) g_camera.y += move_speed; // S
    if (keys[3]) g_camera.x += move_speed; // D
}

void camera_update_with_sprint(int keys[4], int sprint_active) {
    float move_speed = CAMERA_SPEED / g_camera.zoom;
    
    // Double speed when sprinting
    if (sprint_active) {
        move_speed *= 2.5f;
    }
    
    if (keys[0]) g_camera.y -= move_speed; // W
    if (keys[1]) g_camera.x -= move_speed; // A
    if (keys[2]) g_camera.y += move_speed; // S
    if (keys[3]) g_camera.x += move_speed; // D
}

void camera_zoom(float zoom_delta, int mouse_x, int mouse_y) {
    // Get world position before zoom
    float world_x_before, world_y_before;
    camera_screen_to_world(mouse_x, mouse_y, &world_x_before, &world_y_before);
    
    // Apply zoom with limits
    g_camera.zoom *= (1.0f + zoom_delta);
    
    // Apply zoom limits
    if (g_camera.zoom < 0.1f) g_camera.zoom = 0.1f;   // Min zoom (far out)
    if (g_camera.zoom > 0.75f) g_camera.zoom = 0.75f; // Max zoom (close in)
    
    // Adjust camera to keep world position under mouse cursor
    float world_x_after, world_y_after;
    camera_screen_to_world(mouse_x, mouse_y, &world_x_after, &world_y_after);
    g_camera.x += world_x_before - world_x_after;
    g_camera.y += world_y_before - world_y_after;
}

void camera_screen_to_world(int screen_x, int screen_y, float* world_x, float* world_y) {
    *world_x = (screen_x - WINDOW_WIDTH / 2.0f) / g_camera.zoom + g_camera.x;
    *world_y = (screen_y - WINDOW_HEIGHT / 2.0f) / g_camera.zoom + g_camera.y;
}

void camera_world_to_screen(float world_x, float world_y, int* screen_x, int* screen_y) {
    *screen_x = (int)((world_x - g_camera.x) * g_camera.zoom + WINDOW_WIDTH / 2.0f);
    *screen_y = (int)((world_y - g_camera.y) * g_camera.zoom + WINDOW_HEIGHT / 2.0f);
}

float camera_get_zoom(void) {
    return g_camera.zoom;
}

void camera_get_viewport_bounds(float* left, float* top, float* right, float* bottom) {
    camera_screen_to_world(0, 0, left, top);
    camera_screen_to_world(WINDOW_WIDTH, WINDOW_HEIGHT, right, bottom);
}