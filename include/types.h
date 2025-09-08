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
    float mobility_factor;    // How moveable nodes are (0.0 = static, 1.0 = fully mobile)
    int age_mature;           // Age at which plant stops growing/branching (in frames)
    
    // Nutrition depletion settings
    float nutrition_depletion_strength;  // Base depletion amount per growth
    float nutrition_depletion_radius;    // Radius of depletion effect
    
    // Oxygen production settings
    float oxygen_production_factor;      // Oxygen production intensity (0.0 to 1.0+)
    float oxygen_production_radius;      // Radius of oxygen production effect
    
    // Eating properties
    float nutrition_value;               // How much nutrition this plant provides when eaten
    
    // Node colors (RGB 0-255)
    int node_r, node_g, node_b;
    
    // Chain colors (RGB 0-255)
    int chain_r, chain_g, chain_b;
    
    int active;
} PlantType;

// Fish type configuration
typedef struct {
    char name[MAX_NAME_LENGTH];
    float max_speed;              // Maximum movement speed
    float acceleration;           // How quickly fish can change speed
    float turn_rate;              // How quickly fish can change direction
    float mass;                   // Affects physics interactions
    float size_radius;            // Visual size (affects rendering)
    
    // Eating parameters
    float eating_range;           // Distance within which fish can eat plants
    float eating_rate;            // How often fish tries to eat (per frame)
    float digestion_rate;         // How quickly fish processes food into energy
    
    // RL Vision parameters
    float fov_range;              // How far the fish can see
    float fov_angle;              // Field of view angle in radians (total angle)
    
    // Node colors (RGB 0-255)
    int node_r, node_g, node_b;
    
    int active;
} FishType;

// Node structure (used for both plants and fish)
typedef struct {
    float x, y;       // Position
    float vx, vy;     // Velocity
    int active;       // Active flag
    int can_grow;     // Growth capability flag (only for plants)
    int plant_type;   // Plant type index (-1 for fish nodes)
    int branch_count; // Current branch count (only for plants)
    int age;          // Age in frames
} Node;

// Fish structure (extends Node concept but separate)
typedef struct {
    int node_id;                  // Associated node in simulation
    int fish_type;                // Fish type index
    float movement_force_x;       // Current movement force
    float movement_force_y;       // Current movement force
    float energy;                 // Fish energy level (0.0 to 1.0)
    float stomach_contents;       // Current food in stomach (0.0 to 1.0)
    int last_eating_frame;        // Frame when fish last ate
    int age;                      // Age in frames
    int active;                   // Active flag
    
    // RL state and rewards
    float vision_rays[8];         // 8 vision rays showing distance to nearest obstacle
    float hunger_level;           // Computed hunger (0.0 = full, 1.0 = starving)
    float saturation_level;       // Computed saturation (0.0 = empty stomach, 1.0 = full)
    float total_reward;           // Accumulated reward for this fish
    float last_reward;            // Reward from last action
    
    // RL action space
    float desired_turn;           // Desired turning direction (-1.0 to 1.0)
    float desired_speed;          // Desired speed (0.0 to 1.0)
} Fish;

// Chain structure connecting two nodes
typedef struct {
    int node1, node2;
    int active;
    int plant_type;   // Plant type for rendering
    int age;          // Age in frames
    float curve_strength; // Curve strength (-1.0 to 1.0)
    float curve_offset;   // Perpendicular offset for curve direction
} Chain;

// Camera for viewport control
typedef struct {
    float x, y;       // World position
    float zoom;       // Zoom level
} Camera;

// Spatial grid cell for optimization
typedef struct {
    int node_indices[MAX_NODES_PER_CELL];
    int count;
} GridCell;

#endif // TYPES_H