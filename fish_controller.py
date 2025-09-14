#!/usr/bin/env python3
"""
Enhanced Reinforcement Learning Fish Controller v4 for Marine Ecosystem Simulation.
Features 180° vision, chemoreceptors for nutrition detection, and balanced reward system.
"""

import simulation
import random
import math
import numpy as np

# Global state
fish_ids = []
frame_counter = 0

# Enhanced RL Network with 180° vision & chemoreceptors
class ChemoreceptorFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        
        # Get vision system info from C
        vision_rays, nutrition_rays = simulation.get_vision_info()
        
        # Enhanced state: 12 vision + 12 nutrition + oxygen + hunger + saturation = 27 inputs!
        # Actions: turn (-1 to 1), speed (0 to 1) = 2 outputs
        self.vision_rays = vision_rays
        self.nutrition_rays = nutrition_rays
        self.state_size = vision_rays + nutrition_rays + 3  # 12 + 12 + 3 = 27
        self.action_size = 2
        
        # Deeper neural network for complex sensory processing (27 -> 32 -> 16 -> 8 -> 2)
        self.weights1 = np.random.randn(self.state_size, 32) * 0.3
        self.weights2 = np.random.randn(32, 16) * 0.3
        self.weights3 = np.random.randn(16, 8) * 0.3
        self.weights4 = np.random.randn(8, self.action_size) * 0.3
        self.bias1 = np.zeros(32)
        self.bias2 = np.zeros(16)
        self.bias3 = np.zeros(8)
        self.bias4 = np.zeros(self.action_size)
        
        # Balanced learning parameters
        self.learning_rate = 0.012
        self.exploration_rate = 0.4  # Moderate exploration
        self.exploration_decay = 0.9998
        self.min_exploration = 0.06
        
        # Experience tracking
        self.last_state = None
        self.last_action = None
        self.total_reward = 0.0
        self.oxygen_history = []
        self.hunger_history = []
        self.nutrition_detection_history = []
        
    def sigmoid(self, x):
        return
    
    # Get vision system info
    vision_rays, nutrition_rays = simulation.get_vision_info()
    print(f"Vision system: {vision_rays} vision rays, {nutrition_rays} chemoreceptor rays (180° coverage)")
    
    # Spawn 15 fish randomly across the world
    for i in range(15):
        x = random.uniform(world_left + 500, world_right - 500)
        y = random.uniform(world_top + 500, world_bottom - 500)
        fish_type = random.randint(0, fish_type_count - 1)
        
        fish_id = simulation.fish_add(x, y, fish_type)
        if fish_id >= 0:
            fish_ids.append(fish_id)
            fish_brains[fish_id] = ChemoreceptorFishBrain(fish_id)
            print(f"Created chemoreceptor RL fish {fish_id} at ({x:.1f}, {y:.1f}) with {vision_rays}+{nutrition_rays} sensors")

def print_chemoreceptor_rl_debug_info():
    """Print detailed chemoreceptor RL learning progress"""
    print(f"\n=== CHEMORECEPTOR RL FISH STATUS (Frame {frame_counter}) ===")
    
    total_oxygen = 0.0
    total_hunger = 0.0
    total_rewards = 0.0
    total_nutrition_detected = 0.0
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
            
            # Calculate current nutrition detection
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
                total_nutrition_detected += current_nutrition
                
                # Check for critical conditions
                if oxygen < 0.3 or hunger > 0.8:
                    critical_fish.append((fish_id, oxygen, hunger))
                
                # Check for successful fish (good condition + detecting nutrition)
                if oxygen > 0.7 and hunger < 0.4 and current_nutrition > 0.5:
                    successful_fish.append((fish_id, current_nutrition, brain.total_reward))
                
                print(f"Fish {fish_id}: Energy={energy:.2f}, O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                      f"Saturation={saturation:.2f}, NutrDetected={current_nutrition:.3f} (max_ray={max_nutrition_ray:.3f}), "
                      f"Reward={last_reward:.3f}, Total_Reward={brain.total_reward:.2f}, Exploration={brain.exploration_rate:.3f}")
                
                # Print sensory trends
                if len(brain.nutrition_detection_history) > 20:
                    recent_nutrition_trend = brain.nutrition_detection_history[-1] - brain.nutrition_detection_history[-20]
                    recent_oxygen_trend = brain.oxygen_history[-1] - brain.oxygen_history[-20]
                    print(f"  Trends: Nutrition_change={recent_nutrition_trend:.3f}, O2_change={recent_oxygen_trend:.3f}")
                
                # Print best nutrition rays (directional info)
                best_rays = []
                for i in range(brain.nutrition_rays):
                    ray_value = simulation.fish_get_nutrition_ray(fish_id, i)
                    if ray_value > 0.2:  # Significant detection
                        direction = "left" if i < brain.nutrition_rays//3 else "center" if i < 2*brain.nutrition_rays//3 else "right"
                        best_rays.append(f"ray{i}({direction})={ray_value:.2f}")
                
                if best_rays:
                    print(f"  Active nutrition rays: {', '.join(best_rays[:3])}")  # Show top 3
    
    # Summary statistics
    active_count = len(fish_ids)
    if active_count > 0:
        avg_oxygen = total_oxygen / active_count
        avg_hunger = total_hunger / active_count
        avg_reward = total_rewards / active_count
        avg_nutrition_detected = total_nutrition_detected / active_count
        
        print(f"\n=== SUMMARY ===")
        print(f"Active fish: {active_count}")
        print(f"Average oxygen: {avg_oxygen:.2f}")
        print(f"Average hunger: {avg_hunger:.2f}")
        print(f"Average nutrition detected: {avg_nutrition_detected:.3f}")
        print(f"Average total reward: {avg_reward:.2f}")
        
        if critical_fish:
            print(f"Critical fish ({len(critical_fish)}): {critical_fish}")
        
        if successful_fish:
            print(f"Successful fish ({len(successful_fish)}): {successful_fish}")
            
        if not critical_fish and not successful_fish:
            print("All fish in moderate condition")
        
        # System health assessment
        if avg_oxygen > 0.6 and avg_hunger < 0.5:
            print("🟢 SYSTEM HEALTHY: Good oxygen and hunger levels")
        elif avg_oxygen < 0.3 or avg_hunger > 0.8:
            print("🔴 SYSTEM CRITICAL: Low oxygen or high hunger")
        else:
            print("🟡 SYSTEM MODERATE: Monitoring needed")
        
        print("=" * 60)

def update_fish():
    """Enhanced update function with chemoreceptor systems"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
        vision_rays, nutrition_rays = simulation.get_vision_info()
        print("Enhanced RL Fish Controller v4: Chemoreceptor Fish System Initialized!")
        print(f"Features: {vision_rays} vision rays (180°), {nutrition_rays} nutrition detection rays")
        print("Enhanced: Reduced oxygen consumption, chemoreceptor rewards, guided exploration")
        print("Press 'R' to toggle fish vision ray rendering!")
    
    # Update each fish brain every frame
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Print enhanced debug info every 25 seconds
    if frame_counter % (60 * 25) == 0:
        print_chemoreceptor_rl_debug_info()

# This function will be called from the C code each frame
if __name__ == "__main__":
    print("Enhanced RL Fish Controller v4 with 180° Vision & Chemoreceptors loaded!") 1.0 / (1.0 + np.exp(-np.clip(x, -500, 500)))
    
    def tanh(self, x):
        return np.tanh(np.clip(x, -10, 10))
    
    def relu(self, x):
        return np.maximum(0, x)
    
    def leaky_relu(self, x, alpha=0.01):
        return np.where(x > 0, x, alpha * x)
    
    def forward(self, state):
        """Enhanced forward pass through deeper network"""
        # First layer: tanh activation for vision processing
        hidden1 = np.dot(state, self.weights1) + self.bias1
        hidden1 = self.tanh(hidden1)
        
        # Second layer: leaky relu for feature extraction
        hidden2 = np.dot(hidden1, self.weights2) + self.bias2
        hidden2 = self.leaky_relu(hidden2)
        
        # Third layer: relu for decision processing
        hidden3 = np.dot(hidden2, self.weights3) + self.bias3
        hidden3 = self.relu(hidden3)
        
        # Output layer
        output = np.dot(hidden3, self.weights4) + self.bias4
        
        # Turn action: tanh for range -1 to 1
        turn_action = self.tanh(output[0])
        # Speed action: sigmoid for range 0 to 1
        speed_action = self.sigmoid(output[1])
        
        return np.array([turn_action, speed_action]), hidden1, hidden2, hidden3
    
    def get_state(self):
        """Get enhanced state vector with 180° vision & chemoreceptors"""
        state = np.zeros(self.state_size)
        
        # Vision rays (12 values) - 180° field of view
        for i in range(self.vision_rays):
            state[i] = simulation.fish_get_vision_ray(self.fish_id, i)
        
        # Chemoreceptor rays (12 values) - nutrition detection
        for i in range(self.nutrition_rays):
            state[self.vision_rays + i] = simulation.fish_get_nutrition_ray(self.fish_id, i)
        
        # Core state variables
        base_idx = self.vision_rays + self.nutrition_rays
        state[base_idx] = simulation.fish_get_oxygen_level(self.fish_id)
        state[base_idx + 1] = simulation.fish_get_hunger_level(self.fish_id)
        state[base_idx + 2] = simulation.fish_get_saturation_level(self.fish_id)
        
        return state
    
    def choose_action(self, state):
        """Enhanced action selection with chemoreceptor guidance"""
        if random.random() < self.exploration_rate:
            # Exploration with chemoreceptor bias
            oxygen_level = state[self.vision_rays + self.nutrition_rays]
            hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
            
            # Calculate total nutrition detected from chemoreceptors
            total_nutrition = sum(state[self.vision_rays:self.vision_rays + self.nutrition_rays])
            max_nutrition_ray = max(state[self.vision_rays:self.vision_rays + self.nutrition_rays])
            
            # Emergency oxygen behavior
            if oxygen_level < 0.3:
                turn_action = random.uniform(-0.6, 0.6)  # Conservative turning
                speed_action = random.uniform(0.7, 1.0)   # Active movement for oxygen
            
            # High hunger + nutrition detected = guided search
            elif hunger_level > 0.6 and max_nutrition_ray > 0.3:
                # Find the ray with highest nutrition
                best_ray = np.argmax(state[self.vision_rays:self.vision_rays + self.nutrition_rays])
                
                # Convert ray index to turn direction
                # Ray 0 = far left, Ray 6 = center, Ray 11 = far right
                center_ray = self.nutrition_rays // 2
                turn_bias = (best_ray - center_ray) / center_ray  # -1 to 1
                
                # Bias turning toward nutrition source
                turn_action = turn_bias * 0.8 + random.uniform(-0.3, 0.3)
                speed_action = random.uniform(0.6, 1.0)  # Active pursuit
                
                turn_action = np.clip(turn_action, -1.0, 1.0)
            
            # Normal exploration
            else:
                turn_action = random.uniform(-1.0, 1.0)
                speed_action = random.uniform(0.4, 1.0)
            
            action = np.array([turn_action, speed_action])
        else:
            # Exploit: use network
            action, _, _, _ = self.forward(state)
            # Ensure minimum movement
            if action[1] < 0.25:
                action[1] = 0.25
        
        return action
    
    def learn(self, state, action, reward, next_state):
        """Enhanced learning with chemoreceptor reward integration"""
        if self.last_state is None:
            return
        
        # Enhanced reward analysis
        oxygen_level = state[self.vision_rays + self.nutrition_rays]
        hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
        next_oxygen = next_state[self.vision_rays + self.nutrition_rays]
        next_hunger = next_state[self.vision_rays + self.nutrition_rays + 1]
        
        # Analyze nutrition detection improvement
        current_nutrition = sum(state[self.vision_rays:self.vision_rays + self.nutrition_rays])
        next_nutrition = sum(next_state[self.vision_rays:self.vision_rays + self.nutrition_rays])
        
        # Calculate target with enhanced reward shaping
        current_q, h1, h2, h3 = self.forward(state)
        next_q, _, _, _ = self.forward(next_state)
        
        target = action.copy()
        
        # Enhanced reward-based learning
        if reward > 2.0:  # Very good reward (eating high-nutrition food)
            target = target * (1.0 + reward * 0.08)
        elif reward > 0.5:  # Good reward
            target = target * (1.0 + reward * 0.05)
        elif reward < -0.05:  # Bad reward
            # Analyze what went wrong and adjust
            if next_oxygen < oxygen_level - 0.05:  # Significant oxygen loss
                target[0] = -target[0] * 0.8  # Turn away
                target[1] = min(1.0, target[1] * 1.2)  # Speed up
            elif next_hunger > hunger_level + 0.02:  # Hunger increased significantly
                target[1] = min(1.0, target[1] * 1.1)  # Speed up food search
            else:  # Other penalties
                target[0] = target[0] * 0.8
                target[1] = target[1] * 0.9
        
        # Chemoreceptor-specific learning adjustments
        if next_nutrition > current_nutrition and reward > 0:
            # Good job following nutrition gradient
            target[1] = min(1.0, target[1] * 1.05)  # Encourage similar speed
        elif current_nutrition > 0.5 and reward < 0:
            # Had good nutrition signal but got penalty - reconsider approach
            target[0] = target[0] * 0.9  # Slight turn adjustment
        
        # Emergency overrides
        if oxygen_level < 0.2:
            target[1] = min(1.0, target[1] * 1.3)  # Emergency speed
        if hunger_level > 0.8 and current_nutrition > 0.3:
            # Very hungry with nutrition detected - pursue aggressively
            target[1] = min(1.0, target[1] * 1.2)
        
        # Clamp target
        target[0] = np.clip(target[0], -1.0, 1.0)
        target[1] = np.clip(target[1], 0.0, 1.0)
        
        # Enhanced backpropagation for 4-layer network
        error = target - current_q
        
        # Output layer gradients
        d_weights4 = np.outer(h3, error) * self.learning_rate
        d_bias4 = error * self.learning_rate
        
        # Third hidden layer gradients
        h3_error = np.dot(error, self.weights4.T)
        h3_error = h3_error * (h3 > 0).astype(float)  # ReLU derivative
        
        d_weights3 = np.outer(h2, h3_error) * self.learning_rate
        d_bias3 = h3_error * self.learning_rate
        
        # Second hidden layer gradients
        h2_error = np.dot(h3_error, self.weights3.T)
        h2_error = np.where(h2 > 0, h2_error, 0.01 * h2_error)  # Leaky ReLU derivative
        
        d_weights2 = np.outer(h1, h2_error) * self.learning_rate
        d_bias2 = h2_error * self.learning_rate
        
        # First hidden layer gradients
        h1_error = np.dot(h2_error, self.weights2.T)
        h1_error = h1_error * (1 - h1 * h1)  # tanh derivative
        
        d_weights1 = np.outer(state, h1_error) * self.learning_rate
        d_bias1 = h1_error * self.learning_rate
        
        # Update weights
        self.weights4 += d_weights4
        self.bias4 += d_bias4
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
        """Enhanced update with chemoreceptor monitoring"""
        current_state = self.get_state()
        
        # Track vital signs and sensory input
        oxygen_level = current_state[self.vision_rays + self.nutrition_rays]
        hunger_level = current_state[self.vision_rays + self.nutrition_rays + 1]
        total_nutrition_detected = sum(current_state[self.vision_rays:self.vision_rays + self.nutrition_rays])
        
        self.oxygen_history.append(oxygen_level)
        self.hunger_history.append(hunger_level)
        self.nutrition_detection_history.append(total_nutrition_detected)
        
        # Keep only recent history
        if len(self.oxygen_history) > 300:  # 5 seconds at 60 FPS
            self.oxygen_history.pop(0)
            self.hunger_history.pop(0)
            self.nutrition_detection_history.pop(0)
        
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
        
        # Enhanced debug output for first fish
        if self.fish_id == 0 and simulation.fish_get_count() > 0:
            position = simulation.fish_get_position(self.fish_id)
            if position:
                x, y = position
                if hasattr(self, 'last_position'):
                    dx = x - self.last_position[0]
                    dy = y - self.last_position[1]
                    movement = math.sqrt(dx*dx + dy*dy)
                    if frame_counter % 90 == 0:  # Every 1.5 seconds
                        max_nutrition = max(current_state[self.vision_rays:self.vision_rays + self.nutrition_rays])
                        print(f"Fish {self.fish_id}: O2={oxygen_level:.2f}, Hunger={hunger_level:.2f}, "
                              f"NutritionDetected={total_nutrition_detected:.3f} (max={max_nutrition:.3f}), "
                              f"Action=({action[0]:.2f},{action[1]:.2f}), Reward={reward:.3f}, Movement={movement:.2f}")
                self.last_position = (x, y)

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create initial fish population with enhanced chemoreceptor brains"""
    global fish_ids, fish_brains
    
    # Get world boundaries
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")