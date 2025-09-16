#!/usr/bin/env python3
"""
RL Fish Controller - 3 Input, 3 Output Neural Network
Inputs: plant_vector_x, plant_vector_y, oxygen_level
Outputs: turn_direction, movement_strength, eat_command
"""

import simulation
import random
import math
import numpy as np

# Global state
fish_ids = []
frame_counter = 0

class RLFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        
        # Get RL system info from C
        input_size, output_size = simulation.get_rl_info()
        
        self.input_size = input_size      # 4: plant_vec_x, plant_vec_y, oxygen, plant_distance
        self.output_size = output_size    # 3: turn, movement, eat
        
        print(f"Creating RL brain for fish {fish_id}: {input_size} inputs -> {output_size} outputs")
        
        # Simple but effective 3-layer network (adjusted for 4 inputs)
        self.weights1 = np.random.randn(self.input_size, 20) * 0.3
        self.weights2 = np.random.randn(20, 12) * 0.3
        self.weights3 = np.random.randn(12, self.output_size) * 0.3
        self.bias1 = np.zeros(20)
        self.bias2 = np.zeros(12)
        self.bias3 = np.zeros(self.output_size)
        
        # RL parameters
        self.learning_rate = 0.02
        self.exploration_rate = 0.5
        self.exploration_decay = 0.999
        self.min_exploration = 0.1
        
        # Experience tracking
        self.last_state = None
        self.last_action = None
        self.total_reward = 0.0
        self.frames_alive = 0
        
        # Performance tracking
        self.successful_eats = 0
        self.failed_eats = 0
        self.movement_episodes = 0
        
        # Action persistence for smoother behavior
        self.action_persistence = 0
        self.persistent_action = None
        self.min_persistence_frames = 15  # ~0.25 seconds at 60fps
        
    def sigmoid(self, x):
        return 1.0 / (1.0 + np.exp(-np.clip(x, -500, 500)))
    
    def tanh(self, x):
        return np.tanh(np.clip(x, -10, 10))
    
    def relu(self, x):
        return np.maximum(0, x)
    
    def forward(self, state):
        """Forward pass through neural network"""
        # Layer 1
        hidden1 = np.dot(state, self.weights1) + self.bias1
        hidden1 = self.relu(hidden1)
        
        # Layer 2
        hidden2 = np.dot(hidden1, self.weights2) + self.bias2
        hidden2 = self.tanh(hidden2)
        
        # Layer 3 - Output layer
        output = np.dot(hidden2, self.weights3) + self.bias3
        
        # Apply appropriate activations for each output
        turn_direction = self.tanh(output[0])        # -1.0 to 1.0
        movement_strength = self.sigmoid(output[1])  # 0.0 to 1.0
        eat_command = self.sigmoid(output[2])        # 0.0 to 1.0
        
        processed_output = np.array([turn_direction, movement_strength, eat_command])
        
        return processed_output, hidden1, hidden2
    
    def get_state(self):
        """Get RL state from simulation"""
        inputs = simulation.fish_get_rl_inputs(self.fish_id)
        if inputs is None:
            return np.zeros(self.input_size)
        
        plant_vector_x, plant_vector_y, oxygen_level, plant_distance = inputs
        
        # State vector: [plant_vector_x, plant_vector_y, oxygen_level, plant_distance]
        state = np.array([plant_vector_x, plant_vector_y, oxygen_level, plant_distance])
        
        return state
    
    def choose_action_intelligent(self, state):
        """Intelligent action selection based on state"""
        plant_vector_x, plant_vector_y, oxygen_level, plant_distance = state
        
        # Calculate plant distance and direction
        plant_detected = math.sqrt(plant_vector_x**2 + plant_vector_y**2) > 0.1
        
        # Smart behavior patterns
        if plant_detected and plant_distance < 0.8:  # Plant detected and reasonably close
            # Calculate turn direction toward plant
            plant_angle = math.atan2(plant_vector_y, plant_vector_x)
            
            # Normalize angle to turn direction (-1 to 1)
            turn_toward_plant = math.tanh(plant_angle)
            
            # Movement strength based on distance (closer = slower for precision)
            if plant_distance < 0.3:  # Very close
                movement_strength = 0.3
                eat_command = 0.9  # Try to eat
            elif plant_distance < 0.6:  # Moderately close
                movement_strength = 0.6
                eat_command = 0.4  # Consider eating
            else:  # Detected but far
                movement_strength = 0.8
                eat_command = 0.1  # Focus on approach
                
        else:  # No plant detected or too far - explore
            # Random exploration with some structure
            if random.random() < 0.4:
                # Systematic search pattern
                turn_directions = [-0.8, -0.4, 0.0, 0.4, 0.8]
                turn_toward_plant = random.choice(turn_directions)
            else:
                # Random exploration
                turn_toward_plant = random.uniform(-0.6, 0.6)
            
            movement_strength = 0.6
            eat_command = 0.1
        
        # Oxygen-based adjustments
        if oxygen_level < 0.3:  # Low oxygen
            # Move upward (assuming oxygen is higher up)
            if not plant_detected:  # No strong plant signal
                turn_toward_plant = random.uniform(-0.3, 0.3)  # Slight random turn
            movement_strength = 0.7  # Keep moving to find better oxygen
            
        elif oxygen_level > 0.8:  # High oxygen area
            movement_strength *= 0.8  # Slow down to stay in good area
        
        action = np.array([turn_toward_plant, movement_strength, eat_command])
        
        return action        else:
            # Normal exploration: MUCH calmer
            if random.random() < 0.7:
                directions = [
                    (0, -0.5),    # gentle up
                    (0, 0.5),     # gentle down  
                    (-0.5, 0),    # gentle left
                    (0.5, 0),     # gentle right
                ]
                direction_x, direction_y = random.choice(directions)
            else:
                # Gentle diagonal movement
                direction_x = random.choice([-0.4, -0.2, 0.2, 0.4])
                direction_y = random.choice([-0.4, -0.2, 0.2, 0.4])
        
        action = np.array([direction_x, direction_y])
        
        return action
    
    def choose_action(self, state):
        """Main action selection with exploration vs exploitation"""
        # Action persistence for smoother behavior
        if self.action_persistence > 0:
            self.action_persistence -= 1
            if self.persistent_action is not None:
                return self.persistent_action.copy()
        
        # Exploration vs Exploitation
        if random.random() < self.exploration_rate:
            # Exploration: use intelligent heuristics
            action = self.choose_action_intelligent(state)
            
            # Set persistence for smoother exploration
            self.action_persistence = random.randint(5, self.min_persistence_frames)
            self.persistent_action = action.copy()
            
        else:
            # Exploitation: use neural network
            action, _, _ = self.forward(state)
            
            # Ensure reasonable values
            action[0] = np.clip(action[0], -1.0, 1.0)  # turn_direction
            action[1] = np.clip(action[1], 0.0, 1.0)   # movement_strength
            action[2] = np.clip(action[2], 0.0, 1.0)   # eat_command
            
            # Add slight randomness for exploration
            if random.random() < 0.1:
                action[0] += random.uniform(-0.1, 0.1)
                action[1] += random.uniform(-0.1, 0.1)
                action[2] += random.uniform(-0.1, 0.1)
                
                action[0] = np.clip(action[0], -1.0, 1.0)
                action[1] = np.clip(action[1], 0.0, 1.0)
                action[2] = np.clip(action[2], 0.0, 1.0)
        
        return action
    
    def learn(self, state, action, reward, next_state):
        """Simple Q-learning style update"""
        if self.last_state is None:
            return
        
        # Calculate network output for current state
        current_output, h1, h2 = self.forward(state)
        
        # Calculate target based on reward
        target = action.copy()
        
        # Reward-based learning
        if reward > 0.1:  # Good reward
            # Reinforce successful actions
            learning_strength = min(2.0, reward * 10.0)
            target = action * (1.0 + learning_strength * 0.1)
            
            # Track successful eating
            if action[2] > 0.5:  # Was trying to eat
                self.successful_eats += 1
                
        elif reward < -0.01:  # Penalty
            # Discourage bad actions
            target = action * 0.9
            
            # Track failed eating attempts
            if action[2] > 0.5:  # Was trying to eat
                self.failed_eats += 1
                
        else:  # Neutral reward
            # Small adjustments based on state
            plant_vector_x, plant_vector_y, oxygen_level, plant_distance = state
            
            if plant_distance > 0.7 and action[2] > 0.5:
                # Trying to eat when plant is far - discourage
                target[2] *= 0.95
                
            if oxygen_level > 0.7 and action[1] > 0.8:
                # Moving fast in good oxygen area - encourage staying
                target[1] *= 0.9
        
        # Clamp targets to valid ranges
        target[0] = np.clip(target[0], -1.0, 1.0)
        target[1] = np.clip(target[1], 0.0, 1.0)
        target[2] = np.clip(target[2], 0.0, 1.0)
        
        # Backpropagation
        error = target - current_output
        
        # Adaptive learning rate based on reward magnitude
        adaptive_lr = self.learning_rate * (0.5 + 0.5 * min(1.0, abs(reward) * 10))
        
        # Update output layer
        d_weights3 = np.outer(h2, error) * adaptive_lr
        d_bias3 = error * adaptive_lr
        
        # Update hidden layer 2
        h2_error = np.dot(error, self.weights3.T)
        h2_error = h2_error * (1 - h2 * h2)  # tanh derivative
        h2_error = np.clip(h2_error, -1.0, 1.0)  # Gradient clipping
        
        d_weights2 = np.outer(h1, h2_error) * adaptive_lr
        d_bias2 = h2_error * adaptive_lr
        
        # Update hidden layer 1
        h1_error = np.dot(h2_error, self.weights2.T)
        h1_error = h1_error * (h1 > 0).astype(float)  # ReLU derivative
        h1_error = np.clip(h1_error, -1.0, 1.0)  # Gradient clipping
        
        d_weights1 = np.outer(state, h1_error) * adaptive_lr
        d_bias1 = h1_error * adaptive_lr
        
        # Apply weight updates
        self.weights3 += d_weights3
        self.bias3 += d_bias3
        self.weights2 += d_weights2
        self.bias2 += d_bias2
        self.weights1 += d_weights1
        self.bias1 += d_bias1
        
        # Decay exploration rate
        self.exploration_rate = max(self.min_exploration, 
                                   self.exploration_rate * self.exploration_decay)
    
    def update(self):
        """Main update function"""
        current_state = self.get_state()
        self.frames_alive += 1
        
        # Choose and apply action
        action = self.choose_action(current_state)
        simulation.fish_set_rl_outputs(self.fish_id, action[0], action[1], action[2])
        
        # Get reward and learn
        reward = simulation.fish_get_last_reward(self.fish_id)
        self.total_reward += reward
        
        # Learn from experience
        if self.last_state is not None and self.last_action is not None:
            self.learn(self.last_state, self.last_action, reward, current_state)
        
        # Store experience
        self.last_state = current_state.copy()
        self.last_action = action.copy()
        
        # Debug output for first fish
        if self.fish_id == 0 and hasattr(self, 'debug_counter'):
            self.debug_counter += 1
            
            if self.debug_counter % 180 == 0:  # Every 3 seconds
                plant_vec_x, plant_vec_y, oxygen, plant_distance = current_state
                plant_dist = math.sqrt(plant_vec_x**2 + plant_vec_y**2)
                
                eating_mode = simulation.fish_is_eating(self.fish_id)
                stomach = simulation.fish_get_stomach_contents(self.fish_id)
                
                print(f"Fish {self.fish_id}: PlantDist={plant_distance:.2f}, O2={oxygen:.2f}, "
                      f"Action=({action[0]:.2f},{action[1]:.2f},{action[2]:.2f}), "
                      f"Eating={eating_mode}, Stomach={stomach:.2f}, Reward={reward:.3f}, "
                      f"Explore={self.exploration_rate:.3f}")
        elif self.fish_id == 0:
            self.debug_counter = 0

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create RL fish brains"""
    global fish_ids, fish_brains
    
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")
        return
    
    input_size, output_size = simulation.get_rl_info()
    print(f"RL Fish Controller: {input_size} inputs -> {output_size} outputs")
    print(f"Inputs: plant_vector_x, plant_vector_y, oxygen_level, plant_distance")
    print(f"World size: {world_right-world_left:.0f}x{world_bottom-world_top:.0f}")
    
    existing_fish_count = simulation.fish_get_count()
    print(f"Creating RL brains for {existing_fish_count} fish")
    
    for i in range(existing_fish_count):
        fish_brains[i] = RLFishBrain(i)
        fish_ids.append(i)
        print(f"Created RL brain for fish {i}")

def print_learning_progress():
    """Print learning progress and performance metrics"""
    print(f"\n=== RL FISH LEARNING STATUS (Frame {frame_counter}) ===")
    
    total_rewards = 0.0
    total_exploration = 0.0
    total_successful_eats = 0
    total_failed_eats = 0
    active_brains = 0
    
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            brain = fish_brains[fish_id]
            
            position = simulation.fish_get_position(fish_id)
            stomach = simulation.fish_get_stomach_contents(fish_id)
            eating = simulation.fish_is_eating(fish_id)
            
            if position:
                total_rewards += brain.total_reward
                total_exploration += brain.exploration_rate
                total_successful_eats += brain.successful_eats
                total_failed_eats += brain.failed_eats
                active_brains += 1
                
                print(f"Fish {fish_id}: Stomach={stomach:.2f}, Eating={eating}, "
                      f"Explore={brain.exploration_rate:.3f}, "
                      f"Success/Fail={brain.successful_eats}/{brain.failed_eats}, "
                      f"TotalReward={brain.total_reward:.2f}, Alive={brain.frames_alive}")
    
    # Summary statistics
    if active_brains > 0:
        avg_reward = total_rewards / active_brains
        avg_exploration = total_exploration / active_brains
        eat_success_rate = 0.0
        if (total_successful_eats + total_failed_eats) > 0:
            eat_success_rate = total_successful_eats / (total_successful_eats + total_failed_eats)
        
        print(f"\n=== RL LEARNING SUMMARY ===")
        print(f"Active fish brains: {active_brains}")
        print(f"Average total reward: {avg_reward:.2f}")
        print(f"Average exploration rate: {avg_exploration:.3f}")
        print(f"Total eating attempts: {total_successful_eats + total_failed_eats}")
        print(f"Eating success rate: {eat_success_rate:.2f} ({total_successful_eats}/{total_successful_eats + total_failed_eats})")
        
        # Learning assessment
        if avg_exploration < 0.2 and eat_success_rate > 0.3:
            print("🟢 LEARNING: Fish are learning effectively!")
        elif avg_exploration > 0.4:
            print("🟡 LEARNING: Fish still exploring, early learning phase")
        elif eat_success_rate < 0.1:
            print("🔴 LEARNING: Fish struggling to find food")
        else:
            print("🟡 LEARNING: Fish making progress")
        
        # Get nutrition balance
        fish_consumed, fish_defecated, env_added, env_depleted = simulation.get_nutrition_balance()
        print(f"Nutrition balance: Consumed={fish_consumed:.2f}, Defecated={fish_defecated:.2f}")
        
        print("=" * 60)

def update_fish():
    """Main update with RL learning"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
        print("RL Fish Controller Initialized!")
        print("System: 4 inputs (plant_vector_x, plant_vector_y, oxygen_level, plant_distance)")
        print("Actions: 3 outputs (turn_direction, movement_strength, eat_command)")
        print("Learning: Neural network with exploration/exploitation")
        print("All fish controlled individually with force-based movement!")
    
    # Update each fish brain
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Print progress every 20 seconds
    if frame_counter % (60 * 20) == 0:
        print_learning_progress()

if __name__ == "__main__":
    print("RL Fish Controller loaded!")
    print("Features: 4-input/3-output RL system with turn-based movement")
    print("All fish controlled individually with force-based rigidbody movement!")