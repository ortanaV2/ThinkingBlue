#!/usr/bin/env python3
"""
RL Fish Controller - No NumPy Version
Inputs: plant_vector_x, plant_vector_y, oxygen_level, plant_distance
Outputs: turn_direction, movement_strength, eat_command
"""

import simulation
import random
import math

# Global state
fish_ids = []
frame_counter = 0

class IntelligentFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        print(f"Creating intelligent brain for fish {fish_id} (no NumPy)")
        
        # Simple neural network weights (no NumPy)
        self.input_size = 4
        self.hidden_size = 12
        self.output_size = 3
        
        # Initialize small random weights
        self.weights1 = [[random.uniform(-0.3, 0.3) for _ in range(self.hidden_size)] for _ in range(self.input_size)]
        self.weights2 = [[random.uniform(-0.3, 0.3) for _ in range(self.output_size)] for _ in range(self.hidden_size)]
        self.bias1 = [0.0] * self.hidden_size
        self.bias2 = [0.0] * self.output_size
        
        # Learning parameters
        self.learning_rate = 0.02
        self.exploration_rate = 0.6
        self.exploration_decay = 0.999
        self.min_exploration = 0.1
        
        # Performance tracking
        self.successful_eats = 0
        self.failed_eats = 0
        self.total_reward = 0.0
        self.frames_alive = 0
        
        # Memory for smoother behavior
        self.last_action = [0.0, 0.5, 0.1]
        self.action_persistence = 0
    
    def sigmoid(self, x):
        """Sigmoid activation (0 to 1)"""
        return 1.0 / (1.0 + math.exp(-max(-500, min(500, x))))
    
    def tanh(self, x):
        """Tanh activation (-1 to 1)"""
        return math.tanh(max(-10, min(10, x)))
    
    def relu(self, x):
        """ReLU activation"""
        return max(0, x)
    
    def forward(self, inputs):
        """Forward pass through network"""
        # Hidden layer
        hidden = []
        for i in range(self.hidden_size):
            sum_val = self.bias1[i]
            for j in range(self.input_size):
                sum_val += inputs[j] * self.weights1[j][i]
            hidden.append(self.relu(sum_val))
        
        # Output layer
        outputs = []
        for i in range(self.output_size):
            sum_val = self.bias2[i]
            for j in range(self.hidden_size):
                sum_val += hidden[j] * self.weights2[j][i]
            outputs.append(sum_val)
        
        # Apply activations
        turn_direction = self.tanh(outputs[0])        # -1 to 1
        movement_strength = self.sigmoid(outputs[1])  # 0 to 1
        eat_command = self.sigmoid(outputs[2])        # 0 to 1
        
        return [turn_direction, movement_strength, eat_command], hidden
    
    def get_state(self):
        """Get state from simulation"""
        inputs = simulation.fish_get_rl_inputs(self.fish_id)
        if inputs is None:
            return [0.0, 0.0, 0.0, 1.0]
        return list(inputs)
    
    def choose_action_intelligent(self, state):
        """Rule-based intelligent behavior"""
        plant_vector_x, plant_vector_y, oxygen_level, plant_distance = state
        
        plant_magnitude = math.sqrt(plant_vector_x**2 + plant_vector_y**2)
        plant_detected = plant_magnitude > 0.1
        
        if plant_detected and plant_distance < 0.8:
            # Move toward plant
            plant_angle = math.atan2(plant_vector_y, plant_vector_x)
            turn_direction = math.tanh(plant_angle)
            
            if plant_distance < 0.3:  # Very close
                movement_strength = 0.2
                eat_command = 0.9
            elif plant_distance < 0.6:  # Moderately close
                movement_strength = 0.5
                eat_command = 0.6
            else:  # Far but detected
                movement_strength = 0.8
                eat_command = 0.1
        else:
            # Explore
            if random.random() < 0.3:
                turn_directions = [-0.7, -0.3, 0.0, 0.3, 0.7]
                turn_direction = random.choice(turn_directions)
            else:
                turn_direction = random.uniform(-0.5, 0.5)
            
            movement_strength = 0.6
            eat_command = 0.1
        
        # Oxygen adjustments
        if oxygen_level < 0.3:
            movement_strength = 0.7
            if not plant_detected:
                turn_direction = random.uniform(-0.2, 0.2)
        elif oxygen_level > 0.8:
            movement_strength *= 0.9
        
        return [turn_direction, movement_strength, eat_command]
    
    def choose_action(self, state):
        """Choose action: exploration vs exploitation"""
        if self.action_persistence > 0:
            self.action_persistence -= 1
            return self.last_action[:]
        
        if random.random() < self.exploration_rate:
            # Exploration using intelligent rules
            action = self.choose_action_intelligent(state)
            self.action_persistence = random.randint(8, 15)  # Smoother behavior
        else:
            # Exploitation using neural network
            action, _ = self.forward(state)
            
            # Add small randomness
            if random.random() < 0.15:
                action[0] += random.uniform(-0.1, 0.1)
                action[1] += random.uniform(-0.05, 0.05)
                action[2] += random.uniform(-0.05, 0.05)
        
        # Clamp values
        action[0] = max(-1.0, min(1.0, action[0]))  # turn
        action[1] = max(0.0, min(1.0, action[1]))   # movement
        action[2] = max(0.0, min(1.0, action[2]))   # eat
        
        self.last_action = action[:]
        return action
    
    def learn(self, state, action, reward):
        """Simple learning without NumPy"""
        if reward > 0.1:
            # Good reward
            if action[2] > 0.5:  # Was eating
                self.successful_eats += 1
            
            # Simple weight adjustment toward successful action
            outputs, hidden = self.forward(state)
            for i in range(self.output_size):
                error = action[i] - outputs[i]
                self.bias2[i] += error * self.learning_rate * 0.1
                
                for j in range(self.hidden_size):
                    self.weights2[j][i] += hidden[j] * error * self.learning_rate * 0.1
        
        elif reward < -0.01:
            # Bad reward
            if action[2] > 0.5:  # Failed eating
                self.failed_eats += 1
        
        # Decay exploration
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
        self.learn(current_state, action, reward)
        
        # Debug for first fish
        if self.fish_id == 0 and hasattr(self, 'debug_counter'):
            self.debug_counter += 1
            if self.debug_counter % 180 == 0:  # Every 3 seconds
                plant_distance = current_state[3]
                oxygen = current_state[2]
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
    """Create intelligent fish brains"""
    global fish_ids, fish_brains
    
    existing_fish_count = simulation.fish_get_count()
    print(f"Creating intelligent brains for {existing_fish_count} fish (no NumPy)")
    
    for i in range(existing_fish_count):
        fish_brains[i] = IntelligentFishBrain(i)
        fish_ids.append(i)
        print(f"Created intelligent brain for fish {i}")

def print_learning_progress():
    """Print learning progress"""
    print(f"\n=== INTELLIGENT FISH STATUS (Frame {frame_counter}) ===")
    
    active_brains = 0
    total_rewards = 0.0
    total_successful_eats = 0
    total_failed_eats = 0
    total_exploration = 0.0
    
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            brain = fish_brains[fish_id]
            position = simulation.fish_get_position(fish_id)
            
            if position:
                active_brains += 1
                total_rewards += brain.total_reward
                total_successful_eats += brain.successful_eats
                total_failed_eats += brain.failed_eats
                total_exploration += brain.exploration_rate
                
                stomach = simulation.fish_get_stomach_contents(fish_id)
                eating = simulation.fish_is_eating(fish_id)
                
                print(f"Fish {fish_id}: Stomach={stomach:.2f}, Eating={eating}, "
                      f"Success/Fail={brain.successful_eats}/{brain.failed_eats}, "
                      f"TotalReward={brain.total_reward:.2f}, Explore={brain.exploration_rate:.3f}")
    
    if active_brains > 0:
        avg_reward = total_rewards / active_brains
        avg_exploration = total_exploration / active_brains
        eat_attempts = total_successful_eats + total_failed_eats
        success_rate = 0.0
        if eat_attempts > 0:
            success_rate = total_successful_eats / eat_attempts
        
        print(f"\n=== LEARNING SUMMARY ===")
        print(f"Active fish: {active_brains}")
        print(f"Average reward: {avg_reward:.2f}")
        print(f"Average exploration: {avg_exploration:.3f}")
        print(f"Eating success rate: {success_rate:.2f} ({total_successful_eats}/{eat_attempts})")
        
        # Learning status
        if avg_exploration < 0.2 and success_rate > 0.4:
            print("🟢 LEARNING: Fish are learning well!")
        elif avg_exploration > 0.5:
            print("🟡 LEARNING: Still exploring")
        else:
            print("🟡 LEARNING: Making progress")
        
        # Nutrition balance
        fish_consumed, fish_defecated, env_added, env_depleted = simulation.get_nutrition_balance()
        print(f"Nutrition: Consumed={fish_consumed:.2f}, Defecated={fish_defecated:.2f}")
    
    print("=" * 60)

def update_fish():
    """Main update function"""
    global frame_counter
    frame_counter += 1
    
    # Initialize on first call
    if frame_counter == 1:
        initialize_fish()
        print("Intelligent Fish Controller Initialized!")
        print("Features: Rule-based + Simple Neural Network (no NumPy)")
        print("Learning: Exploration/Exploitation with reward-based adaptation")
    
    # Update each fish
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Progress every 20 seconds
    if frame_counter % (60 * 20) == 0:
        print_learning_progress()

if __name__ == "__main__":
    print("Intelligent Fish Controller loaded!")
    print("Features: Hybrid rule-based + neural network (no external dependencies)")