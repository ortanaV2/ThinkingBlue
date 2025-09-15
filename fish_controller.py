#!/usr/bin/env python3
"""
Simplified Fish Controller with Directional Movement for Marine Ecosystem Simulation.
Features 4-directional movement (up, down, left, right) with constant max speed.
"""

import simulation
import random
import math
import numpy as np

# Global state
fish_ids = []
frame_counter = 0

# Simplified RL Network for directional movement
class DirectionalFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        
        # Get vision system info from C
        vision_rays, nutrition_rays = simulation.get_vision_info()
        
        # State: 12 vision + 12 nutrition + oxygen + hunger + saturation = 27 inputs
        # Actions: direction_x (-1 to 1), direction_y (-1 to 1) = 2 outputs
        self.vision_rays = vision_rays
        self.nutrition_rays = nutrition_rays
        self.state_size = vision_rays + nutrition_rays + 3  # 12 + 12 + 3 = 27
        self.action_size = 2  # direction_x, direction_y
        
        # Simplified neural network (27 -> 24 -> 12 -> 2)
        self.weights1 = np.random.randn(self.state_size, 24) * 0.4
        self.weights2 = np.random.randn(24, 12) * 0.4
        self.weights3 = np.random.randn(12, self.action_size) * 0.4
        self.bias1 = np.zeros(24)
        self.bias2 = np.zeros(12)
        self.bias3 = np.zeros(self.action_size)
        
        # Enhanced learning parameters for simpler system
        self.learning_rate = 0.02  # Higher learning rate
        self.exploration_rate = 0.6  # More exploration initially
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
        """Forward pass through simplified network"""
        # First layer: ReLU for feature extraction
        hidden1 = np.dot(state, self.weights1) + self.bias1
        hidden1 = self.relu(hidden1)
        
        # Second layer: tanh for direction processing
        hidden2 = np.dot(hidden1, self.weights2) + self.bias2
        hidden2 = self.tanh(hidden2)
        
        # Output layer: tanh for direction outputs (-1 to 1)
        output = np.dot(hidden2, self.weights3) + self.bias3
        direction_x = self.tanh(output[0])
        direction_y = self.tanh(output[1])
        
        return np.array([direction_x, direction_y]), hidden1, hidden2
    
    def get_state(self):
        """Get enhanced state vector with vision & chemoreceptors"""
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
        """Enhanced action selection with guaranteed movement"""
        if random.random() < self.exploration_rate:
            # Exploration with chemoreceptor guidance
            oxygen_level = state[self.vision_rays + self.nutrition_rays]
            hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
            
            # Calculate nutrition detection
            nutrition_rays = state[self.vision_rays:self.vision_rays + self.nutrition_rays]
            max_nutrition_ray = max(nutrition_rays)
            best_nutrition_ray_idx = np.argmax(nutrition_rays)
            
            # Emergency oxygen behavior - move randomly but actively
            if oxygen_level < 0.3:
                direction_x = random.uniform(-1.0, 1.0)
                direction_y = random.uniform(-1.0, 1.0)
            
            # High hunger + nutrition detected = move toward nutrition
            elif hunger_level > 0.6 and max_nutrition_ray > 0.3:
                # Convert ray index to direction bias
                center_ray = self.nutrition_rays // 2
                direction_bias_x = (best_nutrition_ray_idx - center_ray) / center_ray  # -1 to 1
                
                # Move toward detected nutrition with some randomness
                direction_x = direction_bias_x * 0.8 + random.uniform(-0.3, 0.3)
                direction_y = random.uniform(-0.5, 0.5)  # Allow vertical movement
                
                # Clamp to valid range
                direction_x = np.clip(direction_x, -1.0, 1.0)
                direction_y = np.clip(direction_y, -1.0, 1.0)
            
            # Normal exploration - ALWAYS generate strong movement
            else:
                # 80% chance for pure cardinal direction, 20% for diagonal
                if random.random() < 0.8:
                    # Pure cardinal directions (up, down, left, right) - STRONG movement
                    directions = [
                        (0, -1),  # up
                        (0, 1),   # down
                        (-1, 0),  # left
                        (1, 0),   # right
                    ]
                    direction_x, direction_y = random.choice(directions)
                else:
                    # Strong diagonal movement
                    direction_x = random.choice([-1.0, -0.7, 0.7, 1.0])
                    direction_y = random.choice([-1.0, -0.7, 0.7, 1.0])
            
            action = np.array([direction_x, direction_y])
        else:
            # Exploit: use network
            action, _, _ = self.forward(state)
            
            # ENSURE minimum movement strength
            magnitude = np.sqrt(action[0]**2 + action[1]**2)
            if magnitude < 0.3:  # Too weak movement
                # Amplify to minimum strength
                if magnitude > 0.01:
                    action = action / magnitude * 0.5  # Scale to 50% strength
                else:
                    # Random direction if action is near zero
                    angle = random.random() * 2 * math.pi
                    action = np.array([math.cos(angle) * 0.5, math.sin(angle) * 0.5])
        
        # FINAL CHECK: Ensure action has sufficient magnitude
        magnitude = np.sqrt(action[0]**2 + action[1]**2)
        if magnitude < 0.2:
            # Force strong movement
            angle = random.random() * 2 * math.pi
            action = np.array([math.cos(angle), math.sin(angle)])
        
        return action
    
    def learn(self, state, action, reward, next_state):
        """Enhanced learning for directional movement"""
        if self.last_state is None:
            return
        
        # Calculate target action based on reward feedback
        current_action, h1, h2 = self.forward(state)
        
        target = action.copy()
        
        # Reward-based learning adjustments
        if reward > 1.0:  # Good reward (eating)
            # Reinforce current action
            target = action * 1.05
        elif reward > 0.1:  # Moderate reward
            # Slightly reinforce
            target = action * 1.02
        elif reward < -0.05:  # Penalty
            # Analyze what went wrong and adjust
            oxygen_level = state[self.vision_rays + self.nutrition_rays]
            hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
            
            if oxygen_level < 0.5:  # Low oxygen - try different direction
                target[0] = -action[0] * 0.7  # Reverse X direction
                target[1] = action[1] * 0.9   # Keep Y direction mostly
            elif hunger_level > 0.7:  # Very hungry - be more active
                # Amplify movement
                target = action * 1.1
            else:
                # General penalty - slight direction change
                target = action * 0.9
        
        # Chemoreceptor-based learning
        nutrition_rays = state[self.vision_rays:self.vision_rays + self.nutrition_rays]
        next_nutrition_rays = next_state[self.vision_rays:self.vision_rays + self.nutrition_rays]
        
        nutrition_improvement = sum(next_nutrition_rays) - sum(nutrition_rays)
        if nutrition_improvement > 0.1 and reward > 0:
            # Good job following nutrition gradient
            target = action * 1.03
        elif sum(nutrition_rays) > 0.5 and reward < 0:
            # Had nutrition signal but got penalty - try different approach
            target[0] = target[0] * 0.8 + random.uniform(-0.2, 0.2)
            target[1] = target[1] * 0.8 + random.uniform(-0.2, 0.2)
        
        # Clamp target to valid range
        target[0] = np.clip(target[0], -1.0, 1.0)
        target[1] = np.clip(target[1], -1.0, 1.0)
        
        # Backpropagation for 3-layer network
        error = target - current_action
        
        # Output layer gradients
        d_weights3 = np.outer(h2, error) * self.learning_rate
        d_bias3 = error * self.learning_rate
        
        # Second hidden layer gradients
        h2_error = np.dot(error, self.weights3.T)
        h2_error = h2_error * (1 - h2 * h2)  # tanh derivative
        
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
        """Enhanced update with directional movement tracking"""
        current_state = self.get_state()
        
        # Track movement patterns
        position = simulation.fish_get_position(self.fish_id)
        if position:
            self.movement_history.append(position)
            if len(self.movement_history) > 60:  # Keep 1 second of history
                self.movement_history.pop(0)
        
        # Choose and apply action
        action = self.choose_action(current_state)
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
            if hasattr(self, 'debug_counter'):
                self.debug_counter += 1
            else:
                self.debug_counter = 0
                
            if self.debug_counter % 120 == 0:  # Every 2 seconds
                oxygen = current_state[self.vision_rays + self.nutrition_rays]
                hunger = current_state[self.vision_rays + self.nutrition_rays + 1]
                max_nutrition = max(current_state[self.vision_rays:self.vision_rays + self.nutrition_rays])
                
                print(f"Fish {self.fish_id}: O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                      f"MaxNutrition={max_nutrition:.3f}, Action=({action[0]:.2f},{action[1]:.2f}), "
                      f"Reward={reward:.3f}, Exploration={self.exploration_rate:.3f}")

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create initial fish population with directional movement brains"""
    global fish_ids, fish_brains
    
    # Get world boundaries
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")
        return
    
    # Get vision system info
    vision_rays, nutrition_rays = simulation.get_vision_info()
    print(f"Vision system: {vision_rays} vision rays, {nutrition_rays} nutrition rays")
    
    # Spawn 15 fish randomly across the world
    for i in range(15):
        x = random.uniform(world_left + 500, world_right - 500)
        y = random.uniform(world_top + 500, world_bottom - 500)
        fish_type = random.randint(0, fish_type_count - 1)
        
        fish_id = simulation.fish_add(x, y, fish_type)
        if fish_id >= 0:
            fish_ids.append(fish_id)
            fish_brains[fish_id] = DirectionalFishBrain(fish_id)
            print(f"Created directional fish {fish_id} at ({x:.1f}, {y:.1f})")

def print_directional_debug_info():
    """Print detailed directional movement learning progress"""
    print(f"\n=== DIRECTIONAL FISH STATUS (Frame {frame_counter}) ===")
    
    total_oxygen = 0.0
    total_hunger = 0.0
    total_rewards = 0.0
    total_movement = 0.0
    critical_fish = []
    successful_fish = []
    
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            brain = fish_brains[fish_id]
            energy = simulation.fish_get_energy(fish_id)
            oxygen = simulation.fish_get_oxygen_level(fish_id)
            hunger = simulation.fish_get_hunger_level(fish_id)
            saturation = simulation.fish_get_saturation_level(fish_id)
            last_reward = simulation.fish_get_last_reward(fish_id)
            
            # Calculate movement activity
            velocity = simulation.fish_get_velocity(fish_id)
            current_speed = 0.0
            if velocity:
                vx, vy = velocity
                current_speed = math.sqrt(vx*vx + vy*vy)
            
            # Calculate nutrition detection
            current_nutrition = 0.0
            max_nutrition_ray = 0.0
            for i in range(brain.nutrition_rays):
                ray_value = simulation.fish_get_nutrition_ray(fish_id, i)
                current_nutrition += ray_value
                max_nutrition_ray = max(max_nutrition_ray, ray_value)
            
            if energy is not None:
                total_oxygen += oxygen
                total_hunger += hunger
                total_rewards += brain.total_reward
                total_movement += current_speed
                
                # Check for critical conditions
                if oxygen < 0.3 or hunger > 0.8:
                    critical_fish.append((fish_id, oxygen, hunger, current_speed))
                
                # Check for successful fish
                if oxygen > 0.7 and hunger < 0.4 and current_nutrition > 0.5:
                    successful_fish.append((fish_id, current_nutrition, current_speed))
                
                print(f"Fish {fish_id}: Energy={energy:.2f}, O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                      f"Speed={current_speed:.2f}, NutrDetected={current_nutrition:.3f}, "
                      f"Reward={last_reward:.3f}, TotalReward={brain.total_reward:.2f}, "
                      f"Exploration={brain.exploration_rate:.3f}")
    
    # Summary statistics
    active_count = len(fish_ids)
    if active_count > 0:
        avg_oxygen = total_oxygen / active_count
        avg_hunger = total_hunger / active_count
        avg_reward = total_rewards / active_count
        avg_movement = total_movement / active_count
        
        print(f"\n=== SUMMARY ===")
        print(f"Active fish: {active_count}")
        print(f"Average movement speed: {avg_movement:.2f}")
        print(f"Average oxygen: {avg_oxygen:.2f}")
        print(f"Average hunger: {avg_hunger:.2f}")
        print(f"Average total reward: {avg_reward:.2f}")
        
        if critical_fish:
            print(f"Critical fish ({len(critical_fish)}): {critical_fish}")
        
        if successful_fish:
            print(f"Successful fish ({len(successful_fish)}): {successful_fish}")
            
        # Movement assessment
        if avg_movement > 6.0:
            print("🟢 MOVEMENT: Fish are swimming actively at good speed")
        elif avg_movement > 3.0:
            print("🟡 MOVEMENT: Fish are moving at moderate speed")
        else:
            print("🔴 MOVEMENT: Fish are moving too slowly")
        
        # System health assessment
        if avg_oxygen > 0.6 and avg_hunger < 0.5:
            print("🟢 SYSTEM HEALTHY: Good oxygen and hunger levels")
        elif avg_oxygen < 0.3 or avg_hunger > 0.8:
            print("🔴 SYSTEM CRITICAL: Low oxygen or high hunger")
        else:
            print("🟡 SYSTEM MODERATE: Monitoring needed")
        
        print("=" * 60)

def update_fish():
    """Enhanced update function with directional movement system"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
        vision_rays, nutrition_rays = simulation.get_vision_info()
        print("Simplified Directional Fish Controller Initialized!")
        print(f"Features: {vision_rays} vision rays, {nutrition_rays} nutrition rays")
        print("Movement: 4-directional (up/down/left/right) with constant max speed")
        print("Press 'R' to toggle fish vision ray rendering!")
    
    # Update each fish brain every frame
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Print debug info every 25 seconds
    if frame_counter % (60 * 25) == 0:
        print_directional_debug_info()

# This function will be called from the C code each frame
if __name__ == "__main__":
    print("Simplified Directional Fish Controller loaded!")