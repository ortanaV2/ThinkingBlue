#ifndef TYPES_H
#define TYPES_H

#include <math.h>

// Core constants
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define MAX_NODES 10000000
#define MAX_CHAINS 10000000
#define MAX_PLANT_TYPES 64
#define MAX_NAME_LENGTH 64

// Fish constants
#define MAX_FISH 50000
#define MAX_FISH_TYPES 32

// Enhanced vision system
#define VISION_RAYS 12           // 12 rays for 180° vision coverage
#define CHEMORECEPTOR_RAYS 12    // 12 rays for nutrition detection

// Layer resolution (shared by nutrition and gas layers)
#define LAYER_GRID_SIZE 30.0f

// Physics parameters
#define OPTIMAL_DISTANCE 50.0f
#define REPULSION_FORCE 0.05f
#define CHAIN_FORCE 0.05f
#define WATER_DRAG 0.95f

// Rendering parameters
#define NODE_RADIUS 5
#define CHAIN_THICKNESS 6

// Camera parameters
#define CAMERA_SPEED 5.0f
#define ZOOM_SPEED 0.1f
#define MIN_ZOOM 0.01f
#define MAX_ZOOM 5.0f

// World configuration
#define WORLD_WIDTH 15000.0f
#define WORLD_HEIGHT 15000.0f
#define WORLD_CENTER_X 0.0f
#define WORLD_CENTER_Y 0.0f

// Derived world bounds
#define WORLD_LEFT (WORLD_CENTER_X - WORLD_WIDTH / 2.0f)
#define WORLD_RIGHT (WORLD_CENTER_X + WORLD_WIDTH / 2.0f)
#define WORLD_TOP (WORLD_CENTER_Y - WORLD_HEIGHT / 2.0f)
#define WORLD_BOTTOM (WORLD_CENTER_Y + WORLD_HEIGHT / 2.0f)

// Spatial optimization
#define GRID_SIZE 40.0f
#define MAX_NODES_PER_CELL 200

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Plant type configuration
typedef struct {
    char name[MAX_NAME_LENGTH];
    float growth_probability;
    int growth_attempts;
    int max_branches;
    float branch_distance;
    float mobility_factor;
    int age_mature;
    
    // Nutrition depletion settings
    float nutrition_depletion_strength;
    float nutrition_depletion_radius;
    
    // Oxygen production settings
    float oxygen_production_factor;
    float oxygen_production_radius;
    
    // Node colors (RGB 0-255)
    int node_r, node_g, node_b;
    
    // Chain colors (RGB 0-255)
    int chain_r, chain_g, chain_b;
    
    int active;
} PlantType;

// SIMPLIFIED: Fish type configuration with constant speed movement
typedef struct {
    char name[MAX_NAME_LENGTH];
    float max_speed;             // Always move at this speed when active
    float acceleration;          // How quickly to reach max speed
    float turn_rate;             // Not used in new system
    float mass;
    float size_radius;
    
    // Eating parameters
    float eating_range;
    float eating_rate;
    float digestion_rate;
    
    // Defecation parameters for nutrition cycle
    float defecation_rate;
    float defecation_radius;
    
    // Enhanced vision parameters
    float fov_range;
    float fov_angle;
    float chemoreceptor_range;
    
    // Oxygen consumption
    float oxygen_consumption_rate;
    float oxygen_refill_rate;
    
    // Flow field interaction
    float flow_sensitivity;
    
    // Node colors (RGB 0-255)
    int node_r, node_g, node_b;
    
    int active;
} FishType;

// Node structure
typedef struct {
    float x, y;
    float vx, vy;
    int active;
    int can_grow;
    int plant_type;   // Plant type index (-1 for fish nodes)
    int branch_count;
    int age;
    
    // Kept for compatibility, but always 0
    float nutrition_cost;
} Node;

// SIMPLIFIED: Fish structure with direct movement control
typedef struct {
    int node_id;
    int fish_type;
    
    // SIMPLIFIED: Direct movement direction from NN
    float desired_direction_x;    // NN output: -1 to 1
    float desired_direction_y;    // NN output: -1 to 1
    float movement_force_x;       // Calculated force
    float movement_force_y;       // Calculated force
    
    float energy;
    float stomach_contents;
    float consumed_nutrition;
    int last_eating_frame;
    int last_defecation_frame;
    int age;
    int active;
    
    // Enhanced RL state with chemoreceptors
    float oxygen_level;
    float hunger_level;
    float vision_rays[VISION_RAYS];
    float nutrition_rays[CHEMORECEPTOR_RAYS];
    float saturation_level;
    float total_reward;
    float last_reward;
} Fish;

// Chain structure connecting two nodes
typedef struct {
    int node1, node2;
    int active;
    int plant_type;
    int age;
    float curve_strength;
    float curve_offset;
} Chain;

// Camera for viewport control
typedef struct {
    float x, y;
    float zoom;
} Camera;

// Spatial grid cell for optimization
typedef struct {
    int node_indices[MAX_NODES_PER_CELL];
    int count;
} GridCell;

#endif // TYPES_H