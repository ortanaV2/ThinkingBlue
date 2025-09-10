#!/usr/bin/env python3
"""
Reinforcement Learning Fish Controller v2 for Marine Ecosystem Simulation.
Each fish uses RL to learn optimal behavior for survival and feeding.
Enhanced with better movement rewards and punishments.
"""

import simulation
import random
import math
import numpy as np

# Global state
fish_ids = []
frame_counter = 0

# Simple RL Network (Q-Learning with function approximation)
class SimpleFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        # State: 8 vision rays + hunger + saturation = 10 inputs
        # Actions: turn (-1 to 1), speed (0 to 1) = 2 outputs
        self.state_size = 10
        self.action_size = 2
        
        # Simple neural network weights (10 -> 16 -> 2)
        self.weights1 = np.random.randn(self.state_size, 16) * 0.5
        self.weights2 = np.random.randn(16, self.action_size) * 0.5
        self.bias1 = np.zeros(16)
        self.bias2 = np.zeros(self.action_size)
        
        # Learning parameters
        self.learning_rate = 0.01
        self.exploration_rate = 0.4  # Higher initial exploration
        self.exploration_decay = 0.9998
        self.min_exploration = 0.05
        
        # Experience
        self.last_state = None
        self.last_action = None
        self.total_reward = 0.0
        
    def sigmoid(self, x):
        return 1.0 / (1.0 + np.exp(-np.clip(x, -500, 500)))
    
    def tanh(self, x):
        return np.tanh(np.clip(x, -10, 10))
    
    def forward(self, state):
        """Forward pass through the network"""
        # Hidden layer
        hidden = np.dot(state, self.weights1) + self.bias1
        hidden = self.tanh(hidden)
        
        # Output layer
        output = np.dot(hidden, self.weights2) + self.bias2
        
        # Turn action: tanh for range -1 to 1
        turn_action = self.tanh(output[0])
        # Speed action: sigmoid for range 0 to 1
        speed_action = self.sigmoid(output[1])
        
        return np.array([turn_action, speed_action]), hidden
    
    def get_state(self):
        """Get current state vector for the fish"""
        state = np.zeros(self.state_size)
        
        # Vision rays (8 values)
        for i in range(8):
            state[i] = simulation.fish_get_vision_ray(self.fish_id, i)
        
        # Hunger level
        state[8] = simulation.fish_get_hunger_level(self.fish_id)
        
        # Saturation level
        state[9] = simulation.fish_get_saturation_level(self.fish_id)
        
        return state
    
    def choose_action(self, state):
        """Choose action using epsilon-greedy policy with movement bias"""
        if random.random() < self.exploration_rate:
            # Explore: random action - ensure some movement
            turn_action = random.uniform(-1.0, 1.0)
            speed_action = random.uniform(0.4, 1.0)  # Minimum 40% speed for movement
            action = np.array([turn_action, speed_action])
        else:
            # Exploit: use network
            action, _ = self.forward(state)
            # Ensure minimum speed for movement
            if action[1] < 0.3:
                action[1] = 0.3
        
        return action
    
    def learn(self, state, action, reward, next_state):
        """Enhanced gradient-based learning with movement penalties"""
        if self.last_state is None:
            return
            
        # Calculate target
        current_q, hidden = self.forward(state)
        next_q, _ = self.forward(next_state)
        
        # Simple temporal difference learning with reward shaping
        target = action.copy()
        
        # Reward-based action adjustment
        if reward > 0.5:  # Very good reward - strongly reinforce
            target = target * (1.0 + reward * 0.2)
        elif reward > 0.1:  # Good reward - reinforce action
            target = target * (1.0 + reward * 0.1)
        elif reward < -0.02:  # Bad reward - modify action
            if reward < -0.05:  # Very bad (boundary or failed eating)
                target[0] = -target[0] * 0.8  # Strong turn away
                target[1] = target[1] * 0.6   # Reduce speed
            else:  # Mild bad (stillness)
                target[1] = min(1.0, target[1] * 1.2)  # Increase speed
        
        # Clamp target
        target[0] = np.clip(target[0], -1.0, 1.0)
        target[1] = np.clip(target[1], 0.0, 1.0)
        
        # Calculate error
        error = target - current_q
        
        # Backpropagation (simplified)
        # Output layer gradients
        d_weights2 = np.outer(hidden, error) * self.learning_rate
        d_bias2 = error * self.learning_rate
        
        # Hidden layer gradients
        hidden_error = np.dot(error, self.weights2.T)
        hidden_error = hidden_error * (1 - hidden * hidden)  # tanh derivative
        
        d_weights1 = np.outer(state, hidden_error) * self.learning_rate
        d_bias1 = hidden_error * self.learning_rate
        
        # Update weights
        self.weights2 += d_weights2
        self.bias2 += d_bias2
        self.weights1 += d_weights1
        self.bias1 += d_bias1
        
        # Decay exploration
        self.exploration_rate = max(self.min_exploration, 
                                   self.exploration_rate * self.exploration_decay)
    
    def update(self):
        """Main update function called each frame"""
        current_state = self.get_state()
        
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
            position = simulation.fish_get_position(self.fish_id)
            if position:
                x, y = position
                if hasattr(self, 'last_position'):
                    dx = x - self.last_position[0]
                    dy = y - self.last_position[1]
                    movement = math.sqrt(dx*dx + dy*dy)
                    if simulation.fish_get_count() % 60 == 0:  # Every second
                        print(f"Fish {self.fish_id}: Action=({action[0]:.2f},{action[1]:.2f}), Movement={movement:.2f}, Reward={reward:.3f}")
                self.last_position = (x, y)

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create initial fish population with RL brains"""
    global fish_ids, fish_brains
    
    # Get world boundaries
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")
        return
    
    # Spawn 15 fish randomly across the world
    for i in range(15):
        x = random.uniform(world_left + 500, world_right - 500)
        y = random.uniform(world_top + 500, world_bottom - 500)
        fish_type = random.randint(0, fish_type_count - 1)
        
        fish_id = simulation.fish_add(x, y, fish_type)
        if fish_id >= 0:
            fish_ids.append(fish_id)
            fish_brains[fish_id] = SimpleFishBrain(fish_id)
            print(f"Created RL fish {fish_id} at ({x:.1f}, {y:.1f})")

def print_rl_debug_info():
    """Print detailed RL learning progress"""
    print(f"\n=== RL Fish Status (Frame {frame_counter}) ===")
    
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            brain = fish_brains[fish_id]
            energy = simulation.fish_get_energy(fish_id)
            hunger = simulation.fish_get_hunger_level(fish_id)
            saturation = simulation.fish_get_saturation_level(fish_id)
            last_reward = simulation.fish_get_last_reward(fish_id)
            
            if energy is not None:
                print(f"Fish {fish_id}: Energy={energy:.2f}, Hunger={hunger:.2f}, "
                      f"Saturation={saturation:.2f}, Reward={last_reward:.3f}, "
                      f"Total_Reward={brain.total_reward:.2f}, "
                      f"Exploration={brain.exploration_rate:.3f}")
                
                # Print vision information
                vision_str = "Vision: ["
                for i in range(8):
                    ray_value = simulation.fish_get_vision_ray(fish_id, i)
                    vision_str += f"{ray_value:.2f}"
                    if i < 7:
                        vision_str += ", "
                vision_str += "]"
                print(f"  {vision_str}")

def update_fish():
    """Main update function called each frame from C simulation"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
        print("RL Fish Controller v2: All fish initialized with enhanced neural networks!")
        print("Press 'R' to toggle fish vision ray rendering!")
    
    # Update each fish brain every frame for responsive learning
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Print detailed debug info every 15 seconds
    if frame_counter % (60 * 15) == 0:
        print_rl_debug_info()
        
        # Print learning statistics
        if fish_brains:
            avg_reward = sum(brain.total_reward for brain in fish_brains.values()) / len(fish_brains)
            avg_exploration = sum(brain.exploration_rate for brain in fish_brains.values()) / len(fish_brains)
            best_fish_id = max(fish_brains.keys(), key=lambda fid: fish_brains[fid].total_reward)
            print(f"\nAverage Total Reward: {avg_reward:.2f}")
            print(f"Average Exploration Rate: {avg_exploration:.3f}")
            print(f"Best Fish: {best_fish_id} with {fish_brains[best_fish_id].total_reward:.2f} total reward")
            print("=" * 50)

# This function will be called from the C code each frame
if __name__ == "__main__":
    print("RL Fish Controller v2 with Vision Ray Rendering loaded!")