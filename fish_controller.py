#!/usr/bin/env python3
"""
Pure Neural Network Fish Controller with Genetic Inheritance
- 100% neural network control (no hardcoded rules)
- Neural network inheritance from parents to offspring
- Works for both herbivores and predators
- Genetic evolution over generations
"""

import simulation
import random
import math
import copy

# Global state
fish_brains = {}
frame_counter = 0
next_fish_id = 0

class PureNeuralFishBrain:
    def __init__(self, fish_id, parent_brain=None):
        self.fish_id = fish_id
        
        # Network architecture for 7 inputs, 3 outputs
        self.input_size = 7
        self.hidden_size = 20
        self.output_size = 3
        
        if parent_brain:
            # Inherit from parent with mutations
            self.inherit_from_parent(parent_brain)
            print(f"Fish {fish_id} inherited neural network from parent with mutations")
        else:
            # Initialize new random network
            self.initialize_random_network()
            print(f"Fish {fish_id} created with new random neural network")
        
        # Learning parameters
        self.learning_rate = 0.08
        self.exploration_rate = 0.6
        self.exploration_decay = 0.9995
        self.min_exploration = 0.05
        
        # Performance tracking
        self.successful_actions = 0
        self.failed_actions = 0
        self.total_reward = 0.0
        self.frames_alive = 0
        self.generation = parent_brain.generation + 1 if parent_brain else 0
        
        # Memory for experience replay
        self.memory = []
        self.max_memory = 100
        
        # Action consistency
        self.last_outputs = [0.0, 0.5, 0.1]
        self.momentum = 0.15
    
    def initialize_random_network(self):
        # Xavier/Glorot initialization for better learning
        fan_in = self.input_size
        fan_out = self.hidden_size
        limit1 = math.sqrt(6.0 / (fan_in + fan_out))
        
        self.weights1 = [[random.uniform(-limit1, limit1) for _ in range(self.hidden_size)] 
                        for _ in range(self.input_size)]
        
        fan_in = self.hidden_size
        fan_out = self.output_size
        limit2 = math.sqrt(6.0 / (fan_in + fan_out))
        
        self.weights2 = [[random.uniform(-limit2, limit2) for _ in range(self.output_size)] 
                        for _ in range(self.hidden_size)]
        
        self.bias1 = [random.uniform(-0.1, 0.1) for _ in range(self.hidden_size)]
        self.bias2 = [random.uniform(-0.1, 0.1) for _ in range(self.output_size)]
    
    def inherit_from_parent(self, parent_brain):
        # Copy parent's network structure
        self.weights1 = copy.deepcopy(parent_brain.weights1)
        self.weights2 = copy.deepcopy(parent_brain.weights2)
        self.bias1 = copy.deepcopy(parent_brain.bias1)
        self.bias2 = copy.deepcopy(parent_brain.bias2)
        
        # Apply mutations for evolution
        mutation_rate = 0.15
        mutation_strength = 0.3
        
        # Mutate weights1
        for i in range(len(self.weights1)):
            for j in range(len(self.weights1[i])):
                if random.random() < mutation_rate:
                    self.weights1[i][j] += random.uniform(-mutation_strength, mutation_strength)
        
        # Mutate weights2
        for i in range(len(self.weights2)):
            for j in range(len(self.weights2[i])):
                if random.random() < mutation_rate:
                    self.weights2[i][j] += random.uniform(-mutation_strength, mutation_strength)
        
        # Mutate biases
        for i in range(len(self.bias1)):
            if random.random() < mutation_rate:
                self.bias1[i] += random.uniform(-mutation_strength, mutation_strength)
        
        for i in range(len(self.bias2)):
            if random.random() < mutation_rate:
                self.bias2[i] += random.uniform(-mutation_strength, mutation_strength)
        
        # Inherit learning parameters with slight variations
        self.learning_rate = parent_brain.learning_rate * random.uniform(0.8, 1.2)
        self.exploration_rate = parent_brain.exploration_rate * random.uniform(0.9, 1.1)
    
    def sigmoid(self, x):
        x = max(-500, min(500, x))
        return 1.0 / (1.0 + math.exp(-x))
    
    def tanh(self, x):
        x = max(-10, min(10, x))
        return math.tanh(x)
    
    def relu(self, x):
        return max(0, x)
    
    def leaky_relu(self, x, alpha=0.1):
        return max(alpha * x, x)
    
    def forward(self, inputs):
        # Normalize inputs
        normalized_inputs = []
        for i, inp in enumerate(inputs):
            if i < 4:  # Vector components and distances
                normalized_inputs.append(max(-1.0, min(1.0, inp)))
            else:  # Other inputs
                normalized_inputs.append(max(-2.0, min(2.0, inp)))
        
        # Hidden layer with Leaky ReLU activation
        hidden = []
        for i in range(self.hidden_size):
            sum_val = self.bias1[i]
            for j in range(self.input_size):
                sum_val += normalized_inputs[j] * self.weights1[j][i]
            hidden.append(self.leaky_relu(sum_val))
        
        # Output layer
        outputs = []
        for i in range(self.output_size):
            sum_val = self.bias2[i]
            for j in range(self.hidden_size):
                sum_val += hidden[j] * self.weights2[j][i]
            outputs.append(sum_val)
        
        # Apply output activations
        turn_direction = self.tanh(outputs[0])  # -1 to 1
        movement_strength = self.sigmoid(outputs[1])  # 0 to 1
        eat_command = self.sigmoid(outputs[2])  # 0 to 1
        
        return [turn_direction, movement_strength, eat_command], hidden
    
    def get_state(self):
        inputs = simulation.fish_get_rl_inputs(self.fish_id)
        if inputs is None:
            return [0.0, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0]
        return list(inputs)
    
    def choose_action(self, state):
        # Pure neural network decision (no hardcoded rules)
        if random.random() < self.exploration_rate:
            # Exploration: Random actions with some structure
            turn_direction = random.uniform(-1.0, 1.0)
            movement_strength = random.uniform(0.3, 1.0)  # Prefer movement over stillness
            eat_command = random.uniform(0.0, 1.0)
            action = [turn_direction, movement_strength, eat_command]
        else:
            # Exploitation: Use neural network
            action, _ = self.forward(state)
        
        # Apply momentum for smoother behavior
        for i in range(len(action)):
            action[i] = (1 - self.momentum) * action[i] + self.momentum * self.last_outputs[i]
        
        # Clamp outputs
        action[0] = max(-1.0, min(1.0, action[0]))
        action[1] = max(0.0, min(1.0, action[1]))
        action[2] = max(0.0, min(1.0, action[2]))
        
        self.last_outputs = action[:]
        return action
    
    def store_experience(self, state, action, reward, next_state):
        experience = (state, action, reward, next_state)
        self.memory.append(experience)
        
        if len(self.memory) > self.max_memory:
            self.memory.pop(0)  # Remove oldest experience
    
    def learn_from_experience(self, state, action, reward):
        # Backpropagation learning
        if abs(reward) < 0.001:
            return  # Skip learning from zero rewards
        
        # Forward pass to get current outputs
        network_output, hidden = self.forward(state)
        
        # Calculate output errors
        output_errors = []
        for i in range(self.output_size):
            if reward > 0:
                # Positive reward: move network output toward action
                target = action[i]
            else:
                # Negative reward: move network output away from action
                if action[i] > 0.5:
                    target = max(0, action[i] - 0.3)
                else:
                    target = min(1, action[i] + 0.3)
            
            error = target - network_output[i]
            output_errors.append(error)
        
        # Update output layer weights and biases
        learning_rate = self.learning_rate * min(abs(reward) * 5, 3.0)  # Scale by reward magnitude
        
        for i in range(self.output_size):
            # Update bias
            self.bias2[i] += output_errors[i] * learning_rate
            
            # Update weights
            for j in range(self.hidden_size):
                self.weights2[j][i] += hidden[j] * output_errors[i] * learning_rate
        
        # Backpropagate to hidden layer (simplified)
        hidden_errors = []
        for j in range(self.hidden_size):
            error = 0
            for i in range(self.output_size):
                error += output_errors[i] * self.weights2[j][i]
            hidden_errors.append(error)
        
        # Update hidden layer weights and biases
        for j in range(self.hidden_size):
            if hidden[j] > 0:  # Only update if neuron was active (Leaky ReLU derivative)
                # Update bias
                self.bias1[j] += hidden_errors[j] * learning_rate * 0.1
                
                # Update weights
                for i in range(self.input_size):
                    self.weights1[i][j] += state[i] * hidden_errors[j] * learning_rate * 0.1
        
        # Update performance tracking
        if reward > 0.1:
            self.successful_actions += 1
        elif reward < -0.1:
            self.failed_actions += 1
    
    def replay_experience(self):
        # Experience replay for better learning
        if len(self.memory) < 10:
            return
        
        # Sample random experiences
        sample_size = min(5, len(self.memory))
        experiences = random.sample(self.memory, sample_size)
        
        for state, action, reward, next_state in experiences:
            if abs(reward) > 0.01:  # Only replay significant experiences
                self.learn_from_experience(state, action, reward * 0.5)  # Reduced intensity for replay
    
    def update(self):
        current_state = self.get_state()
        self.frames_alive += 1
        
        # Choose action using pure neural network
        action = self.choose_action(current_state)
        
        # Apply action
        simulation.fish_set_rl_outputs(self.fish_id, action[0], action[1], action[2])
        
        # Get reward and learn
        reward = simulation.fish_get_last_reward(self.fish_id)
        self.total_reward += reward
        
        # Learn from current experience
        self.learn_from_experience(current_state, action, reward)
        
        # Store experience for replay
        next_state = self.get_state()  # Get state after action
        self.store_experience(current_state, action, reward, next_state)
        
        # Occasionally replay past experiences
        if self.frames_alive % 10 == 0:
            self.replay_experience()
        
        # Decay exploration rate
        self.exploration_rate = max(self.min_exploration, 
                                   self.exploration_rate * self.exploration_decay)
        
        # Debug output for select fish
        if self.fish_id < 2 and hasattr(self, 'debug_counter'):
            self.debug_counter += 1
            if self.debug_counter % 180 == 0:
                fish_info = simulation.fish_get_type_info(self.fish_id)
                fish_name = fish_info[0] if fish_info else "Unknown"
                is_predator = fish_info[1] if fish_info else 0
                
                print(f"Fish {self.fish_id} ({fish_name}, Gen {self.generation}): "
                      f"Action=({action[0]:.2f},{action[1]:.2f},{action[2]:.2f}), "
                      f"Reward={reward:.3f}, Total={self.total_reward:.1f}, "
                      f"Explore={self.exploration_rate:.3f}, "
                      f"Success={self.successful_actions}, Predator={is_predator}")
        elif self.fish_id < 2:
            self.debug_counter = 0

def create_brain_for_fish(fish_id):
    """Create neural network brain for fish, checking for inheritance"""
    global fish_brains, next_fish_id
    
    # Try to find a parent brain for inheritance
    parent_brain = None
    
    # Get fish type info to determine inheritance strategy
    fish_info = simulation.fish_get_type_info(fish_id)
    if fish_info:
        fish_name = fish_info[0]
        is_predator = fish_info[1]
        
        # Look for successful parent of same species
        best_parent = None
        best_reward = -999999
        
        for existing_fish_id, brain in fish_brains.items():
            # Check if brain belongs to same species type
            existing_fish_info = simulation.fish_get_type_info(existing_fish_id)
            if existing_fish_info:
                if (existing_fish_info[0] == fish_name and  # Same species
                    brain.total_reward > best_reward and     # Better performance
                    brain.frames_alive > 600):               # Mature enough
                    best_parent = brain
                    best_reward = brain.total_reward
        
        parent_brain = best_parent
    
    # Create brain with or without inheritance
    brain = PureNeuralFishBrain(fish_id, parent_brain)
    fish_brains[fish_id] = brain
    
    if fish_id >= next_fish_id:
        next_fish_id = fish_id + 1
    
    return brain

def scan_for_new_fish():
    """Scan for newly spawned fish and create brains for them"""
    current_fish_count = simulation.fish_get_count()
    
    for fish_id in range(current_fish_count):
        if fish_id not in fish_brains:
            # Check if fish is active
            position = simulation.fish_get_position(fish_id)
            if position:
                create_brain_for_fish(fish_id)
                print(f"Created neural network for newly spawned fish {fish_id}")

def print_evolution_progress():
    """Print neural network evolution and learning progress"""
    global frame_counter
    
    print(f"\n=== NEURAL NETWORK EVOLUTION STATUS (Frame {frame_counter}) ===")
    
    active_brains = 0
    total_rewards = 0.0
    total_successful = 0
    total_failed = 0
    total_exploration = 0.0
    generation_counts = {}
    species_performance = {}
    
    for fish_id, brain in fish_brains.items():
        position = simulation.fish_get_position(fish_id)
        if position:
            active_brains += 1
            total_rewards += brain.total_reward
            total_successful += brain.successful_actions
            total_failed += brain.failed_actions
            total_exploration += brain.exploration_rate
            
            # Track generations
            gen = brain.generation
            generation_counts[gen] = generation_counts.get(gen, 0) + 1
            
            # Track species performance
            fish_info = simulation.fish_get_type_info(fish_id)
            if fish_info:
                species = fish_info[0]
                if species not in species_performance:
                    species_performance[species] = {'count': 0, 'total_reward': 0.0, 'avg_gen': 0.0}
                species_performance[species]['count'] += 1
                species_performance[species]['total_reward'] += brain.total_reward
                species_performance[species]['avg_gen'] += gen
    
    if active_brains > 0:
        avg_reward = total_rewards / active_brains
        avg_exploration = total_exploration / active_brains
        
        print(f"Active neural networks: {active_brains}")
        print(f"Average reward: {avg_reward:.1f}")
        print(f"Average exploration rate: {avg_exploration:.3f}")
        print(f"Success/Fail ratio: {total_successful}/{total_failed}")
        
        # Generation distribution
        print("Generation distribution:", dict(sorted(generation_counts.items())))
        
        # Species performance
        print("\nSpecies performance:")
        for species, stats in species_performance.items():
            count = stats['count']
            avg_reward = stats['total_reward'] / count
            avg_gen = stats['avg_gen'] / count
            print(f"  {species}: {count} fish, Avg reward: {avg_reward:.1f}, Avg gen: {avg_gen:.1f}")
        
        # Evolution status
        max_gen = max(generation_counts.keys()) if generation_counts else 0
        total_higher_gen = sum(count for gen, count in generation_counts.items() if gen > 0)
        
        if max_gen >= 3:
            print("🧬 EVOLUTION: Multi-generational learning active!")
        elif max_gen >= 1:
            print("🧬 EVOLUTION: Second generation fish learning!")
        else:
            print("🧬 EVOLUTION: First generation establishing baseline")
        
        print(f"Evolved fish ratio: {total_higher_gen}/{active_brains}")
        
        # Nutrition cycle
        fish_consumed, fish_defecated, env_added, env_depleted = simulation.get_nutrition_balance()
        print(f"Nutrition cycle: In={fish_consumed:.1f}, Out={fish_defecated:.1f}")
    
    print("=" * 80)

def update_fish():
    """Main update function - Pure neural network control"""
    global frame_counter
    frame_counter += 1
    
    # Initialize on first call
    if frame_counter == 1:
        # Create brains for initial fish
        initial_fish_count = simulation.fish_get_count()
        for fish_id in range(initial_fish_count):
            create_brain_for_fish(fish_id)
        
        print("Pure Neural Network Fish Controller Initialized!")
        print("Features: 100% Neural Network + Genetic Inheritance")
        print("No hardcoded rules - everything learned through NN")
    
    # Scan for newly spawned fish (manual spawning or reproduction)
    scan_for_new_fish()
    
    # Update each fish neural network
    active_updates = 0
    for fish_id, brain in list(fish_brains.items()):
        position = simulation.fish_get_position(fish_id)
        if position:
            brain.update()
            active_updates += 1
        else:
            # Fish is dead/inactive, but keep brain for potential inheritance
            pass
    
    # Evolution progress every 30 seconds
    if frame_counter % (60 * 30) == 0:
        print_evolution_progress()

if __name__ == "__main__":
    print("Pure Neural Network Fish Controller with Genetic Evolution loaded!")
    print("Features: 100% NN control + Parent->Child inheritance + Species evolution")