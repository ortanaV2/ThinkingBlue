#ifndef TYPES_H
#define TYPES_H

#include <math.h>

// Core window and simulation constants
#define WINDOW_WIDTH 1700
#define WINDOW_HEIGHT 900
#define MAX_NODES 10000000
#define MAX_CHAINS 10000000
#define MAX_PLANT_TYPES 64
#define MAX_NAME_LENGTH 64

// Fish system constants
#define MAX_FISH 50000
#define MAX_FISH_TYPES 32

// Neural network system (7 inputs, 3 outputs)
#define RL_INPUT_SIZE 7     // target_vector_x, target_vector_y, oxygen_level, target_distance, threat_vector_x, threat_vector_y, danger_level
#define RL_OUTPUT_SIZE 3    // turn_direction, movement_strength, eat_command

// Environmental layer resolution
#define LAYER_GRID_SIZE 30.0f

// Physics simulation parameters
#define OPTIMAL_DISTANCE 50.0f
#define REPULSION_FORCE 0.05f
#define CHAIN_FORCE 0.05f
#define WATER_DRAG 0.95f

// Rendering parameters
#define NODE_RADIUS 5
#define CHAIN_THICKNESS 6

// Camera control
#define CAMERA_SPEED 5.0f
#define ZOOM_SPEED 0.1f

// World configuration
#define WORLD_WIDTH 8000.0f
#define WORLD_HEIGHT 8000.0f
#define WORLD_CENTER_X 0.0f
#define WORLD_CENTER_Y 0.0f

// Initial population settings
#define INITIAL_PLANT_COUNT 300
#define INITIAL_FISH_COUNT 30

// Aging and lifecycle
#define TARGET_FPS 30
#define DEATH_CHECK_INTERVAL 30
#define CORPSE_DECAY_TIME 1800

// Seed immunity system (prevents immediate consumption)
#define SEED_IMMUNITY_TIME 180

// Nutrition system (standardized for all plants)
#define STANDARD_DEPLETION_RANGE 120.0f
#define NUTRITION_RANGE_GRADIENT 0.8f

// Derived world boundaries
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
    
    // Growth parameters
    float growth_probability;
    int growth_attempts;
    int max_branches;
    float branch_distance;
    float mobility_factor;
    int age_mature;
    
    // Environmental impact
    float nutrition_depletion_strength;
    float oxygen_production_factor;
    float oxygen_production_radius;
    
    // Visual configuration
    float node_size_factor;
    float chain_thickness_factor;
    float chain_curvature_factor;
    
    // Colors (RGB 0-255)
    int node_r, node_g, node_b;
    int chain_r, chain_g, chain_b;
    
    int active;
} PlantType;

// Fish type configuration with enhanced visuals
typedef struct {
    char name[MAX_NAME_LENGTH];
    
    // Physical properties
    float max_speed;
    float max_force;
    float mass;
    float size_radius;
    float eating_range;
    
    // Neural network parameters
    float fov_angle;
    float max_turn_angle;
    float oxygen_reward_factor;
    float proximity_reward_factor;
    float eat_punishment;
    
    // Environmental interaction
    float flow_sensitivity;
    
    // Predator-prey system
    float danger_level;
    int is_predator;
    int eating_cooldown_frames;
    float fish_detection_range;
    
    // Lifecycle
    int max_age;
    
    // Visual configuration
    float node_size_factor;
    float tail_length_factor;
    float tail_width_factor;
    
    // Color (RGB 0-255)
    int node_r, node_g, node_b;
    
    int active;
} FishType;

// Simulation node (plants, fish, corpses)
typedef struct {
    float x, y;         // Position
    float vx, vy;       // Velocity
    int active;
    int can_grow;
    int plant_type;     // -1 for fish, -2 for corpse, >=0 for plants
    int branch_count;
    int age;
    
    // Corpse system
    int is_corpse;
    int corpse_decay_timer;
    int original_fish_type;
    float corpse_heading;
    
    // Seed immunity (temporary protection from being eaten)
    int seed_immunity_timer;
    
    // Nutrition storage (per-plant nutrition value)
    float stored_nutrition;
} Node;

// Fish entity with neural network integration
typedef struct {
    int node_id;
    int fish_type;
    
    // Neural network interface
    float heading;
    float rl_inputs[RL_INPUT_SIZE];
    float rl_outputs[RL_OUTPUT_SIZE];
    
    // Fish state
    float energy;
    float stomach_contents;
    float consumed_nutrition;
    int last_eating_frame;
    int age;
    int active;
    
    // Learning tracking
    float total_reward;
    float last_reward;
    
    // Behavior state
    int eating_mode;
    
    // Predator-prey dynamics
    int defecation_count;
    int eating_cooldown;
    int target_fish_id;
    
    // Lifecycle tracking
    int birth_frame;
} Fish;

// Plant chain connection
typedef struct {
    int node1, node2;
    int active;
    int plant_type;
    int age;
    
    // Visual curve parameters
    float curve_strength;
    float curve_offset;
    float curve_multiplier;
} Chain;

// Camera system
typedef struct {
    float x, y;
    float zoom;
} Camera;

// Spatial optimization grid
typedef struct {
    int node_indices[MAX_NODES_PER_CELL];
    int count;
} GridCell;

#endif // TYPES_H