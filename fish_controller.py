#!/usr/bin/env python3
"""
Simple fish controller for marine ecosystem simulation.
This script demonstrates basic fish movement using random forces.
Later this will be replaced with RL agents.
"""

import simulation
import random
import math

# Global state
fish_ids = []
frame_counter = 0

def initialize_fish():
    """Create initial fish population"""
    global fish_ids
    
    # Get world boundaries
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")
        return
    
    # Spawn 20 fish randomly across the world
    for i in range(20):
        x = random.uniform(world_left + 500, world_right - 500)
        y = random.uniform(world_top + 500, world_bottom - 500)
        fish_type = random.randint(0, fish_type_count - 1)
        
        fish_id = simulation.fish_add(x, y, fish_type)
        if fish_id >= 0:
            fish_ids.append(fish_id)
            print(f"Created fish {fish_id} at ({x:.1f}, {y:.1f})")

def random_movement_controller(fish_id):
    """Simple random movement controller for a fish"""
    # Get current fish state
    position = simulation.fish_get_position(fish_id)
    velocity = simulation.fish_get_velocity(fish_id)
    
    if position is None or velocity is None:
        return
    
    x, y = position
    vx, vy = velocity
    
    # Get world boundaries for boundary avoidance
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    
    # Boundary avoidance forces
    boundary_force_x = 0.0
    boundary_force_y = 0.0
    boundary_distance = 800.0
    
    if x < world_left + boundary_distance:
        boundary_force_x = 2.0 * (1.0 - (x - world_left) / boundary_distance)
    elif x > world_right - boundary_distance:
        boundary_force_x = -2.0 * (1.0 - (world_right - x) / boundary_distance)
    
    if y < world_top + boundary_distance:
        boundary_force_y = 2.0 * (1.0 - (y - world_top) / boundary_distance)
    elif y > world_bottom - boundary_distance:
        boundary_force_y = -2.0 * (1.0 - (world_bottom - y) / boundary_distance)
    
    # Random exploration force
    random_force_x = random.uniform(-0.5, 0.5)
    random_force_y = random.uniform(-0.5, 0.5)
    
    # Slight tendency to maintain current direction (momentum)
    momentum_factor = 0.3
    momentum_force_x = vx * momentum_factor
    momentum_force_y = vy * momentum_factor
    
    # Combine forces
    total_force_x = boundary_force_x + random_force_x + momentum_force_x
    total_force_y = boundary_force_y + random_force_y + momentum_force_y
    
    # Apply force to fish
    simulation.fish_set_movement_force(fish_id, total_force_x, total_force_y)

def update_fish():
    """Main update function called each frame from C simulation"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
    
    # Update each fish (run controller every 3 frames for performance)
    if frame_counter % 3 == 0:
        for fish_id in fish_ids:
            random_movement_controller(fish_id)

# This function will be called from the C code each frame
if __name__ == "__main__":
    print("Fish controller loaded - use from simulation")