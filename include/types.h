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

// UPDATED: Extended RL system for predator-prey dynamics
#define RL_INPUT_SIZE 7     // plant_vector_x, plant_vector_y, oxygen_level, plant_distance, foreign_fish_vector_x, foreign_fish_vector_y, danger_level
#define RL_OUTPUT_SIZE 3    // turn_direction, movement_strength, eat_command

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

// World configuration
#define WORLD_WIDTH 8000.0f
#define WORLD_HEIGHT 8000.0f
#define WORLD_CENTER_X 0.0f
#define WORLD_CENTER_Y 0.0f

// Population configuration
#define INITIAL_PLANT_COUNT 70
#define INITIAL_FISH_COUNT 10

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

// UPDATED: Enhanced fish type with predator system
typedef struct {
    char name[MAX_NAME_LENGTH];
    float max_speed;
    float max_force;
    float mass;
    float size_radius;
    
    // Eating parameters
    float eating_range;
    
    // RL-specific parameters
    float fov_angle;
    float max_turn_angle;
    float oxygen_reward_factor;
    float proximity_reward_factor;
    float eat_punishment;
    
    // Flow field interaction
    float flow_sensitivity;
    
    // NEW: Predator system parameters
    float danger_level;                 // How dangerous this fish is (0.0 = herbivore, 1.0 = apex predator)
    int is_predator;                    // 1 = predator (eats fish), 0 = herbivore (eats plants)
    int eating_cooldown_frames;         // Cooldown between eating attempts for predators
    float fish_detection_range;         // Range for detecting other fish
    
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
    
    // Kept for compatibility
    float nutrition_cost;
} Node;

// UPDATED: Enhanced fish structure with predator-prey system
typedef struct {
    int node_id;
    int fish_type;
    
    // RL state and control
    float heading;
    float rl_inputs[RL_INPUT_SIZE];     // Extended inputs
    float rl_outputs[RL_OUTPUT_SIZE];
    
    // Fish state
    float energy;
    float stomach_contents;
    float consumed_nutrition;
    int last_eating_frame;
    int age;
    int active;
    
    // RL tracking
    float total_reward;
    float last_reward;
    
    // Internal state
    int eating_mode;
    
    // NEW: Predator-prey system
    int defecation_count;               // Number of times this fish has defecated (for reproduction)
    int eating_cooldown;                // Frames remaining until can eat again (predators only)
    int target_fish_id;                 // ID of fish being targeted (-1 = none)
    
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