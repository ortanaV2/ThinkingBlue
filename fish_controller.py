#!/usr/bin/env python3
"""
Enhanced Reinforcement Learning Fish Controller v3 for Marine Ecosystem Simulation.
Features oxygen & hunger levels with environmental rewards and nutrition-based eating.
"""

import simulation
import random
import math
import numpy as np

# Global state
fish_ids = []
frame_counter = 0

# Enhanced RL Network with oxygen & hunger inputs
class EnhancedFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        # Enhanced state: 8 vision rays + oxygen + hunger + saturation = 11 inputs
        # Actions: turn (-1 to 1), speed (0 to 1) = 2 outputs
        self.state_size = 11
        self.action_size = 2
        
        # Enhanced neural network (11 -> 20 -> 8 -> 2)
        self.weights1 = np.random.randn(self.state_size, 20) * 0.4
        self.weights2 = np.random.randn(20, 8) * 0.4
        self.weights3 = np.random.randn(8, self.action_size) * 0.4
        self.bias1 = np.zeros(20)
        self.bias2 = np.zeros(8)
        self.bias3 = np.zeros(self.action_size)
        
        # Enhanced learning parameters
        self.learning_rate = 0.015
        self.exploration_rate = 0.5  # Higher exploration for complex environment
        self.exploration_decay = 0.9997
        self.min_exploration = 0.08
        
        # Experience tracking
        self.last_state = None
        self.last_action = None
        self.total_reward = 0.0
        self.oxygen_history = []
        self.hunger_history = []
        
    def sigmoid(self, x):
        return 1.0 / (1.0 + np.exp(-np.clip(x, -500, 500)))
    
    def tanh(self, x):
        return np.tanh(np.clip(x, -10, 10))
    
    def relu(self, x):
        return np.maximum(0, x)
    
    def forward(self, state):
        """Enhanced forward pass through deeper network"""
        # First hidden layer (tanh activation)
        hidden1 = np.dot(state, self.weights1) + self.bias1
        hidden1 = self.tanh(hidden1)
        
        # Second hidden layer (relu activation)
        hidden2 = np.dot(hidden1, self.weights2) + self.bias2
        hidden2 = self.relu(hidden2)
        
        # Output layer
        output = np.dot(hidden2, self.weights3) + self.bias3
        
        # Turn action: tanh for range -1 to 1
        turn_action = self.tanh(output[0])
        # Speed action: sigmoid for range 0 to 1
        speed_action = self.sigmoid(output[1])
        
        return np.array([turn_action, speed_action]), hidden1, hidden2
    
    def get_state(self):
        """Get enhanced state vector with oxygen & hunger"""
        state = np.zeros(self.state_size)
        
        # Vision rays (8 values)
        for i in range(8):
            state[i] = simulation.fish_get_vision_ray(self.fish_id, i)
        
        # Enhanced state variables
        state[8] = simulation.fish_get_oxygen_level(self.fish_id)    # NEW
        state[9] = simulation.fish_get_hunger_level(self.fish_id)    # Enhanced
        state[10] = simulation.fish_get_saturation_level(self.fish_id)
        
        return state
    
    def choose_action(self, state):
        """Enhanced action selection with oxygen & hunger awareness"""
        if random.random() < self.exploration_rate:
            # Exploration with bias toward survival
            oxygen_level = state[8]
            hunger_level = state[9]
            
            # Emergency behaviors for low oxygen or high hunger
            if oxygen_level < 0.3:
                # Low oxygen: seek oxygen-rich areas (more conservative movement)
                turn_action = random.uniform(-0.8, 0.8)
                speed_action = random.uniform(0.6, 1.0)  # Move actively to find oxygen
            elif hunger_level > 0.7:
                # High hunger: seek food aggressively
                turn_action = random.uniform(-1.0, 1.0)
                speed_action = random.uniform(0.7, 1.0)  # Move actively to find food
            else:
                # Normal exploration
                turn_action = random.uniform(-1.0, 1.0)
                speed_action = random.uniform(0.4, 1.0)
            
            action = np.array([turn_action, speed_action])
        else:
            # Exploit: use network
            action, _, _ = self.forward(state)
            # Ensure minimum movement for survival
            if action[1] < 0.3:
                action[1] = 0.3
        
        return action
    
    def learn(self, state, action, reward, next_state):
        """Enhanced learning with oxygen & hunger reward shaping"""
        if self.last_state is None:
            return
        
        # Enhanced reward analysis
        oxygen_level = state[8]
        hunger_level = state[9]
        next_oxygen = next_state[8]
        next_hunger = next_state[9]
        
        # Calculate target with enhanced reward shaping
        current_q, h1, h2 = self.forward(state)
        next_q, _, _ = self.forward(next_state)
        
        target = action.copy()
        
        # Enhanced reward-based learning
        if reward > 1.0:  # Very good reward (eating high-nutrition food)
            target = target * (1.0 + reward * 0.15)
        elif reward > 0.2:  # Good reward
            target = target * (1.0 + reward * 0.08)
        elif reward < -0.1:  # Bad reward (oxygen/hunger penalties)
            # Analyze what went wrong
            if next_oxygen < oxygen_level:  # Oxygen got worse
                target[0] = -target[0] * 0.9  # Turn away strongly
                target[1] = min(1.0, target[1] * 1.3)  # Speed up to find oxygen
            elif next_hunger > hunger_level:  # Hunger got worse
                target[1] = min(1.0, target[1] * 1.2)  # Speed up to find food
            else:  # Other penalties
                target[0] = target[0] * 0.7
                target[1] = target[1] * 0.8
        
        # Oxygen emergency adjustment
        if oxygen_level < 0.2:
            target[1] = min(1.0, target[1] * 1.5)  # Emergency speed increase
        
        # Hunger emergency adjustment
        if hunger_level > 0.8:
            target[1] = min(1.0, target[1] * 1.4)  # Urgent food seeking
        
        # Clamp target
        target[0] = np.clip(target[0], -1.0, 1.0)
        target[1] = np.clip(target[1], 0.0, 1.0)
        
        # Enhanced backpropagation
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
        h1_error = h1_error * (1 - h1 * h1)  # tanh derivative
        
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
        """Enhanced update with oxygen & hunger monitoring"""
        current_state = self.get_state()
        
        # Track vital signs
        oxygen_level = current_state[8]
        hunger_level = current_state[9]
        self.oxygen_history.append(oxygen_level)
        self.hunger_history.append(hunger_level)
        
        # Keep only recent history
        if len(self.oxygen_history) > 300:  # 5 seconds at 60 FPS
            self.oxygen_history.pop(0)
            self.hunger_history.pop(0)
        
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
                    if frame_counter % 60 == 0:  # Every second
                        print(f"Fish {self.fish_id}: O2={oxygen_level:.2f}, Hunger={hunger_level:.2f}, "
                              f"Action=({action[0]:.2f},{action[1]:.2f}), Reward={reward:.3f}, "
                              f"Movement={movement:.2f}")
                self.last_position = (x, y)

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create initial fish population with enhanced RL brains"""
    global fish_ids, fish_brains
    
    # Get world boundaries
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")
        return
    
    # Spawn 12 fish randomly across the world
    for i in range(12):
        x = random.uniform(world_left + 500, world_right - 500)
        y = random.uniform(world_top + 500, world_bottom - 500)
        fish_type = random.randint(0, fish_type_count - 1)
        
        fish_id = simulation.fish_add(x, y, fish_type)
        if fish_id >= 0:
            fish_ids.append(fish_id)
            fish_brains[fish_id] = EnhancedFishBrain(fish_id)
            print(f"Created enhanced RL fish {fish_id} at ({x:.1f}, {y:.1f}) with oxygen & hunger systems")

def print_enhanced_rl_debug_info():
    """Print detailed enhanced RL learning progress"""
    print(f"\n=== ENHANCED RL FISH STATUS (Frame {frame_counter}) ===")
    
    total_oxygen = 0.0
    total_hunger = 0.0
    total_rewards = 0.0
    critical_fish = []
    
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            brain = fish_brains[fish_id]
            energy = simulation.fish_get_energy(fish_id)
            oxygen = simulation.fish_get_oxygen_level(fish_id)
            hunger = simulation.fish_get_hunger_level(fish_id)
            saturation = simulation.fish_get_saturation_level(fish_id)
            last_reward = simulation.fish_get_last_reward(fish_id)
            
            if energy is not None:
                total_oxygen += oxygen
                total_hunger += hunger
                total_rewards += brain.total_reward
                
                # Check for critical conditions
                if oxygen < 0.3 or hunger > 0.8:
                    critical_fish.append((fish_id, oxygen, hunger))
                
                print(f"Fish {fish_id}: Energy={energy:.2f}, O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                      f"Saturation={saturation:.2f}, Reward={last_reward:.3f}, "
                      f"Total_Reward={brain.total_reward:.2f}, Exploration={brain.exploration_rate:.3f}")
                
                # Print recent oxygen/hunger trends
                if len(brain.oxygen_history) > 10:
                    recent_oxygen_trend = brain.oxygen_history[-1] - brain.oxygen_history[-10]
                    recent_hunger_trend = brain.hunger_history[-1] - brain.hunger_history[-10]
                    print(f"  Trends: O2_change={recent_oxygen_trend:.3f}, Hunger_change={recent_hunger_trend:.3f}")
    
    # Summary statistics
    active_count = len(fish_ids)
    if active_count > 0:
        avg_oxygen = total_oxygen / active_count
        avg_hunger = total_hunger / active_count
        avg_reward = total_rewards / active_count
        
        print(f"\n=== SUMMARY ===")
        print(f"Active fish: {active_count}")
        print(f"Average oxygen: {avg_oxygen:.2f}")
        print(f"Average hunger: {avg_hunger:.2f}")
        print(f"Average total reward: {avg_reward:.2f}")
        
        if critical_fish:
            print(f"Critical fish ({len(critical_fish)}): {critical_fish}")
        else:
            print("All fish in good condition!")
        
        print("=" * 50)

def update_fish():
    """Enhanced update function with oxygen & hunger systems"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
        print("Enhanced RL Fish Controller v3: All fish initialized with oxygen & hunger systems!")
        print("Features: Oxygen depletion, hunger increase, environmental rewards, nutrition-based eating")
        print("Press 'R' to toggle fish vision ray rendering!")
    
    # Update each fish brain every frame
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Print enhanced debug info every 20 seconds
    if frame_counter % (60 * 20) == 0:
        print_enhanced_rl_debug_info()

# This function will be called from the C code each frame
if __name__ == "__main__":
    print("Enhanced RL Fish Controller v3 with Oxygen & Hunger Systems loaded!")