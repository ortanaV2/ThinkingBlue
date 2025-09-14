#!/usr/bin/env python3
"""
SIMPLIFIED Reinforcement Learning Fish Controller v5 for Marine Ecosystem Simulation.
Features 180° vision, chemoreceptors, but SIMPLIFIED movement: NN only outputs direction (x,y).
Speed is always constant max_speed when moving.
"""

import simulation
import random
import math
import numpy as np

# Global state
fish_ids = []
frame_counter = 0

# SIMPLIFIED RL Network - direction-only output
class SimplifiedFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        
        # Get vision system info from C
        vision_rays, nutrition_rays = simulation.get_vision_info()
        
        # State: 12 vision + 12 nutrition + oxygen + hunger + saturation = 27 inputs
        # SIMPLIFIED Actions: direction_x, direction_y = 2 outputs (no speed control)
        self.vision_rays = vision_rays
        self.nutrition_rays = nutrition_rays
        self.state_size = vision_rays + nutrition_rays + 3  # 27
        self.action_size = 2  # Only x,y direction
        
        # Simpler neural network for direction control (27 -> 24 -> 16 -> 2)
        self.weights1 = np.random.randn(self.state_size, 24) * 0.4
        self.weights2 = np.random.randn(24, 16) * 0.4
        self.weights3 = np.random.randn(16, self.action_size) * 0.4
        self.bias1 = np.zeros(24)
        self.bias2 = np.zeros(16)
        self.bias3 = np.zeros(self.action_size)
        
        # Simplified learning parameters
        self.learning_rate = 0.015
        self.exploration_rate = 0.5
        self.exploration_decay = 0.9995
        self.min_exploration = 0.1
        
        # Experience tracking
        self.last_state = None
        self.last_action = None
        self.total_reward = 0.0
        self.movement_history = []
        
    def sigmoid(self, x):
        return 1.0 / (1.0 + np.exp(-np.clip(x, -500, 500)))
    
    def tanh(self, x):
        return np.tanh(np.clip(x, -10, 10))
    
    def relu(self, x):
        return np.maximum(0, x)
    
    def forward(self, state):
        """Simplified forward pass - only output direction"""
        # First layer: relu activation
        hidden1 = np.dot(state, self.weights1) + self.bias1
        hidden1 = self.relu(hidden1)
        
        # Second layer: relu activation
        hidden2 = np.dot(hidden1, self.weights2) + self.bias2
        hidden2 = self.relu(hidden2)
        
        # Output layer: tanh for direction (-1 to 1)
        output = np.dot(hidden2, self.weights3) + self.bias3
        
        # Both direction_x and direction_y use tanh: -1 to 1
        direction_x = self.tanh(output[0])
        direction_y = self.tanh(output[1])
        
        return np.array([direction_x, direction_y]), hidden1, hidden2
    
    def get_state(self):
        """Get state vector with vision & chemoreceptors"""
        state = np.zeros(self.state_size)
        
        # Vision rays (12 values)
        for i in range(self.vision_rays):
            state[i] = simulation.fish_get_vision_ray(self.fish_id, i)
        
        # Chemoreceptor rays (12 values)
        for i in range(self.nutrition_rays):
            state[self.vision_rays + i] = simulation.fish_get_nutrition_ray(self.fish_id, i)
        
        # Core state variables
        base_idx = self.vision_rays + self.nutrition_rays
        state[base_idx] = simulation.fish_get_oxygen_level(self.fish_id)
        state[base_idx + 1] = simulation.fish_get_hunger_level(self.fish_id)
        state[base_idx + 2] = simulation.fish_get_saturation_level(self.fish_id)
        
        return state
    
    def choose_action(self, state):
        """Simplified action selection - only direction"""
        if random.random() < self.exploration_rate:
            # SIMPLIFIED Exploration - random direction
            oxygen_level = state[self.vision_rays + self.nutrition_rays]
            hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
            
            # Calculate nutrition detected
            total_nutrition = sum(state[self.vision_rays:self.vision_rays + self.nutrition_rays])
            max_nutrition_ray = max(state[self.vision_rays:self.vision_rays + self.nutrition_rays])
            
            # Emergency oxygen behavior - move actively
            if oxygen_level < 0.3:
                direction_x = random.uniform(-1.0, 1.0)
                direction_y = random.uniform(-1.0, 1.0)
            
            # High hunger + nutrition detected = move toward nutrition
            elif hunger_level > 0.6 and max_nutrition_ray > 0.3:
                # Find ray with highest nutrition
                best_ray = np.argmax(state[self.vision_rays:self.vision_rays + self.nutrition_rays])
                
                # Convert ray index to direction (approximate)
                center_ray = self.nutrition_rays // 2
                ray_bias = (best_ray - center_ray) / center_ray  # -1 to 1
                
                # Move toward nutrition source
                direction_x = ray_bias + random.uniform(-0.3, 0.3)
                direction_y = random.uniform(-0.5, 0.5)
                
                # Clamp to valid range
                direction_x = np.clip(direction_x, -1.0, 1.0)
                direction_y = np.clip(direction_y, -1.0, 1.0)
            
            # Normal random exploration
            else:
                direction_x = random.uniform(-1.0, 1.0)
                direction_y = random.uniform(-1.0, 1.0)
            
            action = np.array([direction_x, direction_y])
        else:
            # Exploit: use neural network
            action, _, _ = self.forward(state)
        
        return action
    
    def learn(self, state, action, reward, next_state):
        """Simplified learning for direction control"""
        if self.last_state is None:
            return
        
        # Enhanced reward shaping
        oxygen_level = state[self.vision_rays + self.nutrition_rays]
        hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
        next_oxygen = next_state[self.vision_rays + self.nutrition_rays]
        next_hunger = next_state[self.vision_rays + self.nutrition_rays + 1]
        
        # Analyze movement and nutrition detection
        current_nutrition = sum(state[self.vision_rays:self.vision_rays + self.nutrition_rays])
        next_nutrition = sum(next_state[self.vision_rays:self.vision_rays + self.nutrition_rays])
        
        # Calculate target direction
        current_q, h1, h2 = self.forward(state)
        next_q, _, _ = self.forward(next_state)
        
        target = action.copy()
        
        # Reward-based direction adjustment
        if reward > 1.0:  # Good reward
            # Strengthen current direction
            target = target * (1.0 + reward * 0.05)
        elif reward < -0.05:  # Bad reward
            # Analyze what went wrong
            if next_oxygen < oxygen_level - 0.03:  # Lost oxygen
                # Try different direction (perpendicular)
                target[0] = -target[1] * 0.8
                target[1] = target[0] * 0.8
            elif next_hunger > hunger_level + 0.02:  # Increased hunger
                # If no nutrition detected, try opposite direction
                if current_nutrition < 0.1:
                    target = -target * 0.7
        
        # Chemoreceptor-based learning
        if next_nutrition > current_nutrition and reward > 0:
            # Good nutrition direction - reinforce
            target = target * 1.03
        elif current_nutrition > 0.3 and reward < 0:
            # Had nutrition signal but got penalty - adjust slightly
            target = target * 0.9 + np.array([random.uniform(-0.2, 0.2), random.uniform(-0.2, 0.2)])
        
        # Emergency overrides
        if oxygen_level < 0.2:
            # Emergency - try random direction
            target = np.array([random.uniform(-1.0, 1.0), random.uniform(-1.0, 1.0)])
        
        # Clamp target to valid range
        target[0] = np.clip(target[0], -1.0, 1.0)
        target[1] = np.clip(target[1], -1.0, 1.0)
        
        # Simplified backpropagation for 3-layer network
        error = target - current_q
        
        # Output layer gradients
        d_weights3 = np.outer(h2, error) * self.learning_rate
        d_bias3 = error * self.learning_rate
        
        # Second hidden layer gradients
        h2_error = np.dot(error, self.weights3.T)
        h2_error = h2_error * (h2 > 0).astype(float)  # ReLU derivative
        
        d_weights2 = np.outer(h1, h2_error) * self.learning_rate
        d_bias2 = h2_error * self.learning_rate
        
        # First hidden layer gradients
        h1_error = np.dot(h2_error, self.weights2.T)
        h1_error = h1_error * (h1 > 0).astype(float)  # ReLU derivative
        
        d_weights1 = np.outer(state, h1_error) * self.learning_rate
        d_bias1 = h1_error * self.learning_rate
        
        # Update weights
        self.weights3 += d_weights3
        self.bias3 += d_bias3
        self.weights2 += d_weights2
        self.bias2 += d_bias2
        self.weights1 += d_weights1
        self.bias1 += d_bias1
        
        # Decay exploration
        self.exploration_rate = max(self.min_exploration, 
                                   self.exploration_rate * self.exploration_decay)
    
    def update(self):
        """Simplified update with direction-only control"""
        current_state = self.get_state()
        
        # Track movement
        position = simulation.fish_get_position(self.fish_id)
        if position:
            self.movement_history.append(position)
            if len(self.movement_history) > 60:  # Keep 1 second of history
                self.movement_history.pop(0)
        
        # Choose action (direction only)
        action = self.choose_action(current_state)
        
        # Apply action - SIMPLIFIED: only direction, speed is constant
        simulation.fish_apply_rl_action(self.fish_id, action[0], action[1])
        
        # Get reward
        reward = simulation.fish_get_last_reward(self.fish_id)
        self.total_reward += reward
        
        # Learn from experience
        if self.last_state is not None and self.last_action is not None:
            self.learn(self.last_state, self.last_action, reward, current_state)
        
        # Store current experience
        self.last_state = current_state.copy()
        self.last_action = action.copy()
        
        # Debug output for first fish
        if self.fish_id == 0 and simulation.fish_get_count() > 0:
            if frame_counter % 120 == 0:  # Every 2 seconds
                oxygen_level = current_state[self.vision_rays + self.nutrition_rays]
                hunger_level = current_state[self.vision_rays + self.nutrition_rays + 1]
                max_nutrition = max(current_state[self.vision_rays:self.vision_rays + self.nutrition_rays])
                
                # Calculate movement over last second
                movement_distance = 0.0
                if len(self.movement_history) > 30:
                    old_pos = self.movement_history[-30]
                    new_pos = self.movement_history[-1]
                    movement_distance = math.sqrt((new_pos[0] - old_pos[0])**2 + (new_pos[1] - old_pos[1])**2)
                
                print(f"Fish {self.fish_id}: O2={oxygen_level:.2f}, Hunger={hunger_level:.2f}, "
                      f"MaxNutrition={max_nutrition:.3f}, Direction=({action[0]:.2f},{action[1]:.2f}), "
                      f"Reward={reward:.3f}, Movement={movement_distance:.1f}, Exploration={self.exploration_rate:.3f}")

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create initial fish population with simplified movement"""
    global fish_ids, fish_brains
    
    # Get world boundaries and fish types
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")
        return
    
    # Get vision system info
    vision_rays, nutrition_rays = simulation.get_vision_info()
    print(f"Vision system: {vision_rays} vision rays, {nutrition_rays} chemoreceptor rays")
    
    # Spawn 15 fish randomly across the world
    for i in range(15):
        x = random.uniform(world_left + 500, world_right - 500)
        y = random.uniform(world_top + 500, world_bottom - 500)
        fish_type = random.randint(0, fish_type_count - 1)
        
        fish_id = simulation.fish_add(x, y, fish_type)
        if fish_id >= 0:
            fish_ids.append(fish_id)
            fish_brains[fish_id] = SimplifiedFishBrain(fish_id)
            print(f"Created SIMPLIFIED RL fish {fish_id} at ({x:.1f}, {y:.1f}) - direction-only control")

def print_simplified_rl_debug_info():
    """Print simplified RL learning progress"""
    print(f"\n=== SIMPLIFIED RL FISH STATUS (Frame {frame_counter}) ===")
    
    total_oxygen = 0.0
    total_hunger = 0.0
    total_rewards = 0.0
    total_movement = 0.0
    active_fish = 0
    
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            brain = fish_brains[fish_id]
            energy = simulation.fish_get_energy(fish_id)
            oxygen = simulation.fish_get_oxygen_level(fish_id)
            hunger = simulation.fish_get_hunger_level(fish_id)
            saturation = simulation.fish_get_saturation_level(fish_id)
            last_reward = simulation.fish_get_last_reward(fish_id)
            
            # Calculate movement over recent history
            movement_distance = 0.0
            if len(brain.movement_history) > 30:
                old_pos = brain.movement_history[-30]
                new_pos = brain.movement_history[-1]
                movement_distance = math.sqrt((new_pos[0] - old_pos[0])**2 + (new_pos[1] - old_pos[1])**2)
            
            if energy is not None:
                total_oxygen += oxygen
                total_hunger += hunger
                total_rewards += brain.total_reward
                total_movement += movement_distance
                active_fish += 1
                
                print(f"Fish {fish_id}: Energy={energy:.2f}, O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                      f"Saturation={saturation:.2f}, Reward={last_reward:.3f}, Total_Reward={brain.total_reward:.2f}, "
                      f"Movement={movement_distance:.1f}, Exploration={brain.exploration_rate:.3f}")
                
                # Show last action
                if brain.last_action is not None:
                    print(f"  Last direction: ({brain.last_action[0]:.3f}, {brain.last_action[1]:.3f})")
    
    # Summary statistics
    if active_fish > 0:
        avg_oxygen = total_oxygen / active_fish
        avg_hunger = total_hunger / active_fish
        avg_reward = total_rewards / active_fish
        avg_movement = total_movement / active_fish
        
        print(f"\n=== SIMPLIFIED MOVEMENT SUMMARY ===")
        print(f"Active fish: {active_fish}")
        print(f"Average oxygen: {avg_oxygen:.2f}")
        print(f"Average hunger: {avg_hunger:.2f}")
        print(f"Average total reward: {avg_reward:.2f}")
        print(f"Average movement distance: {avg_movement:.1f}")
        
        # Movement assessment
        if avg_movement > 50.0:
            print("🟢 MOVEMENT: Fish are actively swimming")
        elif avg_movement > 20.0:
            print("🟡 MOVEMENT: Moderate fish activity")
        else:
            print("🔴 MOVEMENT: Fish are mostly stationary")
        
        # System health
        if avg_oxygen > 0.6 and avg_hunger < 0.5:
            print("🟢 HEALTH: Good oxygen and hunger levels")
        elif avg_oxygen < 0.3 or avg_hunger > 0.8:
            print("🔴 HEALTH: Critical oxygen or hunger levels")
        else:
            print("🟡 HEALTH: Moderate condition")
        
        print("=" * 60)

def update_fish():
    """Simplified update function with direction-only control"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
        vision_rays, nutrition_rays = simulation.get_vision_info()
        print("SIMPLIFIED RL Fish Controller v5: Direction-Only Movement System Initialized!")
        print(f"Features: {vision_rays} vision rays (180°), {nutrition_rays} nutrition detection rays")
        print("SIMPLIFIED: NN outputs only direction (x,y), speed is always constant max_speed")
        print("This should encourage more active movement!")
        print("Press 'R' to toggle fish vision ray rendering!")
    
    # Update each fish brain every frame
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Print debug info every 30 seconds
    if frame_counter % (60 * 30) == 0:
        print_simplified_rl_debug_info()

# This function will be called from the C code each frame
if __name__ == "__main__":
    print("SIMPLIFIED RL Fish Controller v5: Direction-Only Movement System loaded!")
    print("Key change: NN only controls direction, speed is always constant!")