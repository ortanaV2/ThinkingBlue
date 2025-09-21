// types.h - Enhanced with anti-aliasing support and seed immunity system
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

// RL system for predator-prey dynamics
#define RL_INPUT_SIZE 7     // plant_vector_x, plant_vector_y, oxygen_level, plant_distance, foreign_fish_vector_x, foreign_fish_vector_y, danger_level
#define RL_OUTPUT_SIZE 3    // turn_direction, movement_strength, eat_command

// Layer resolution (shared by nutrition and gas layers)
#define LAYER_GRID_SIZE 30.0f

// Physics parameters
#define OPTIMAL_DISTANCE 50.0f
#define REPULSION_FORCE 0.05f
#define CHAIN_FORCE 0.05f
#define WATER_DRAG 0.95f

// Rendering parameters with anti-aliasing support
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
#define INITIAL_PLANT_COUNT 100
#define INITIAL_FISH_COUNT 10

// Fish aging constants
#define TARGET_FPS 30
#define DEATH_CHECK_INTERVAL 30  

// Corpse system constants
#define CORPSE_DECAY_TIME 1800  

// Seed immunity system
#define SEED_IMMUNITY_TIME 180  // 6 seconds at 30 FPS

// Anti-aliasing constants
#define AA_SAMPLE_RADIUS 1.0f    // Subpixel sampling radius
#define AA_ALPHA_THRESHOLD 0.01f // Minimum alpha for pixel rendering

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

// Plant type with visual configuration
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
    
    // Visual configuration
    float node_size_factor;        // Multiplier for node size (default 1.0)
    float chain_thickness_factor;  // Multiplier for chain thickness (default 1.0) 
    float chain_curvature_factor;  // Controls chain curvature strength (default 1.0)
    
    // Node colors (RGB 0-255)
    int node_r, node_g, node_b;
    
    // Chain colors (RGB 0-255)
    int chain_r, chain_g, chain_b;
    
    int active;
} PlantType;

// Fish type configuration
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
    
    // Predator system parameters
    float danger_level;
    int is_predator;
    int eating_cooldown_frames;
    float fish_detection_range;
    
    // Aging system
    int max_age;  // Age in frames when death probability is 50%
    
    // Node colors (RGB 0-255)
    int node_r, node_g, node_b;
    
    int active;
} FishType;

// Node structure with seed immunity system
typedef struct {
    float x, y;
    float vx, vy;
    int active;
    int can_grow;
    int plant_type;   // Plant type index (-1 for fish nodes, -2 for corpse nodes)
    int branch_count;
    int age;
    
    // Corpse system
    int is_corpse;
    int corpse_decay_timer;
    int original_fish_type;  // Fish type that became this corpse
    float corpse_heading;    // Preserved heading from when fish died
    
    // Seed immunity system
    int seed_immunity_timer;  // Frames until seed can be eaten (0 = can be eaten)
    
    // Compatibility
    float nutrition_cost;
} Node;

// Fish structure with aging system
typedef struct {
    int node_id;
    int fish_type;
    
    // RL state and control
    float heading;
    float rl_inputs[RL_INPUT_SIZE];
    float rl_outputs[RL_OUTPUT_SIZE];
    
    // Fish state
    float energy;
    float stomach_contents;
    float consumed_nutrition;
    int last_eating_frame;
    int age;  // Age in frames
    int active;
    
    // RL tracking
    float total_reward;
    float last_reward;
    
    // Internal state
    int eating_mode;
    
    // Predator-prey system
    int defecation_count;
    int eating_cooldown;
    int target_fish_id;
    
    // Aging system
    int birth_frame;  // Frame when fish was born
    
} Fish;

// Chain structure with curvature parameters
typedef struct {
    int node1, node2;
    int active;
    int plant_type;
    int age;
    float curve_strength;    // Base curvature strength
    float curve_offset;      // Random offset for variety
    float curve_multiplier;  // Per-chain multiplier for visual diversity
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