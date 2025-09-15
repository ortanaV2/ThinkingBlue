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
// Zoom limits removed - unlimited zoom range

// World configuration - CONFIGURABLE VALUES
#define WORLD_WIDTH 8000.0f
#define WORLD_HEIGHT 6000.0f
#define WORLD_CENTER_X 0.0f
#define WORLD_CENTER_Y 0.0f

// Population configuration
#define INITIAL_PLANT_COUNT 10
#define INITIAL_FISH_COUNT 5

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

// Fish type configuration with flow sensitivity
typedef struct {
    char name[MAX_NAME_LENGTH];
    float max_speed;
    float acceleration;
    float turn_rate;
    float mass;
    float size_radius;
    
    // Eating parameters
    float eating_range;
    float eating_rate;
    float digestion_rate;
    
    // Defecation parameters for nutrition cycle
    float defecation_rate;        // Probability per frame of defecation
    float defecation_radius;      // Radius for nutrition distribution
    
    // Enhanced vision parameters
    float fov_range;              // How far fish can see
    float fov_angle;              // Field of view angle (now ~3.14 for 180°)
    float chemoreceptor_range;    // How far fish can "smell" nutrition
    
    // Oxygen consumption
    float oxygen_consumption_rate;  // How fast fish consumes oxygen
    float oxygen_refill_rate;       // How fast fish refills oxygen in rich areas
    
    // Flow field interaction
    float flow_sensitivity;         // How much fish are affected by flow (0.0-1.0)
    
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

// Fish structure
typedef struct {
    int node_id;
    int fish_type;
    float movement_force_x;
    float movement_force_y;
    float energy;
    float stomach_contents;
    float consumed_nutrition;     // Total nutrition consumed by this fish
    int last_eating_frame;
    int last_defecation_frame;    // Track when fish last defecated
    int age;
    int active;
    
    // Enhanced RL state with chemoreceptors
    float oxygen_level;                    // Current oxygen level (0.0 to 1.0)
    float hunger_level;                    // Hunger level (0.0 to 1.0)
    float vision_rays[VISION_RAYS];        // Visual obstacle detection (12 rays)
    float nutrition_rays[CHEMORECEPTOR_RAYS]; // Chemical nutrition detection (12 rays)
    float saturation_level;
    float total_reward;
    float last_reward;
    
    // RL action space
    float desired_turn;
    float desired_speed;
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