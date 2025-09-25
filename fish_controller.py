#!/usr/bin/env python3
"""
Neural Network Fish Controller with Model Saving
- Tracks reproduction success for each fish
- Saves best herbivore and predator models on shutdown
- Models can be loaded later for inference
- Updated input system: predators use inputs 0,1,3 for prey targeting
"""

import simulation
import random
import math
import copy
import json
import os
import signal
import sys

# Global state
fish_brains = {}
frame_counter = 0
next_fish_id = 0
shutdown_requested = False

# Performance tracking for model saving
reproduction_tracking = {}  # fish_id -> reproduction_count

class PureNeuralFishBrain:
    def __init__(self, fish_id, parent_brain=None):
        self.fish_id = fish_id
        
        # Network architecture
        self.input_size = 7
        self.hidden_size = 20
        self.output_size = 3
        
        if parent_brain:
            self.inherit_from_parent(parent_brain)
            print(f"Fish {fish_id} inherited neural network from parent with mutations")
        else:
            self.initialize_random_network()
            print(f"Fish {fish_id} created with new random neural network")
        
        # Learning parameters
        self.learning_rate = 0.08
        self.exploration_rate = 0.6
        self.exploration_decay = 0.9995
        self.min_exploration = 0.05
        
        # Performance tracking for model saving
        self.successful_actions = 0
        self.failed_actions = 0
        self.total_reward = 0.0
        self.frames_alive = 0
        self.generation = parent_brain.generation + 1 if parent_brain else 0
        self.reproduction_count = 0  
        self.species_type = None  # Will be set when we know fish type
        self.is_predator = False
        
        # Memory for experience replay
        self.memory = []
        self.max_memory = 100
        
        # Action consistency
        self.last_outputs = [0.0, 0.5, 0.1]
        self.momentum = 0.15
    
    def initialize_random_network(self):
        # Xavier/Glorot initialization
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
        # Copy parent's network
        self.weights1 = copy.deepcopy(parent_brain.weights1)
        self.weights2 = copy.deepcopy(parent_brain.weights2)
        self.bias1 = copy.deepcopy(parent_brain.bias1)
        self.bias2 = copy.deepcopy(parent_brain.bias2)
        
        # Apply mutations
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
        
        # Inherit performance stats
        self.learning_rate = parent_brain.learning_rate * random.uniform(0.8, 1.2)
        self.exploration_rate = parent_brain.exploration_rate * random.uniform(0.9, 1.1)
        self.reproduction_count = parent_brain.reproduction_count + 1
    
    def sigmoid(self, x):
        x = max(-500, min(500, x))
        return 1.0 / (1.0 + math.exp(-x))
    
    def tanh(self, x):
        x = max(-10, min(10, x))
        return math.tanh(x)
    
    def leaky_relu(self, x, alpha=0.1):
        return max(alpha * x, x)
    
    def forward(self, inputs):
        # Normalize inputs
        normalized_inputs = []
        for i, inp in enumerate(inputs):
            if i < 4:
                normalized_inputs.append(max(-1.0, min(1.0, inp)))
            else:
                normalized_inputs.append(max(-2.0, min(2.0, inp)))
        
        # Hidden layer with Leaky ReLU
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
        turn_direction = self.tanh(outputs[0])
        movement_strength = self.sigmoid(outputs[1])
        eat_command = self.sigmoid(outputs[2])
        
        return [turn_direction, movement_strength, eat_command], hidden
    
    def get_state(self):
        inputs = simulation.fish_get_rl_inputs(self.fish_id)
        if inputs is None:
            return [0.0, 0.0, 0.5, 1.0, 0.0, 0.0, 0.0]
        return list(inputs)
    
    def choose_action(self, state):
        if random.random() < self.exploration_rate:
            # Exploration
            turn_direction = random.uniform(-1.0, 1.0)
            movement_strength = random.uniform(0.3, 1.0)
            eat_command = random.uniform(0.0, 1.0)
            action = [turn_direction, movement_strength, eat_command]
        else:
            # Exploitation
            action, _ = self.forward(state)
        
        # Apply momentum
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
            self.memory.pop(0)
    
    def learn_from_experience(self, state, action, reward):
        if abs(reward) < 0.001:
            return
        
        # Forward pass
        network_output, hidden = self.forward(state)
        
        # Calculate output errors
        output_errors = []
        for i in range(self.output_size):
            if reward > 0:
                target = action[i]
            else:
                if action[i] > 0.5:
                    target = max(0, action[i] - 0.3)
                else:
                    target = min(1, action[i] + 0.3)
            
            error = target - network_output[i]
            output_errors.append(error)
        
        # Update weights
        learning_rate = self.learning_rate * min(abs(reward) * 5, 3.0)
        
        # Update output layer
        for i in range(self.output_size):
            self.bias2[i] += output_errors[i] * learning_rate
            
            for j in range(self.hidden_size):
                self.weights2[j][i] += hidden[j] * output_errors[i] * learning_rate
        
        # Backpropagate to hidden layer
        hidden_errors = []
        for j in range(self.hidden_size):
            error = 0
            for i in range(self.output_size):
                error += output_errors[i] * self.weights2[j][i]
            hidden_errors.append(error)
        
        # Update hidden layer
        for j in range(self.hidden_size):
            if hidden[j] > 0:
                self.bias1[j] += hidden_errors[j] * learning_rate * 0.1
                
                for i in range(self.input_size):
                    self.weights1[i][j] += state[i] * hidden_errors[j] * learning_rate * 0.1
        
        # Update performance tracking
        if reward > 0.1:
            self.successful_actions += 1
        elif reward < -0.1:
            self.failed_actions += 1
    
    def replay_experience(self):
        if len(self.memory) < 10:
            return
        
        sample_size = min(5, len(self.memory))
        experiences = random.sample(self.memory, sample_size)
        
        for state, action, reward, next_state in experiences:
            if abs(reward) > 0.01:
                self.learn_from_experience(state, action, reward * 0.5)
    
    def update(self):
        current_state = self.get_state()
        self.frames_alive += 1
        
        # Update species info if not set
        if self.species_type is None:
            fish_info = simulation.fish_get_type_info(self.fish_id)
            if fish_info:
                self.species_type = fish_info[0]
                self.is_predator = fish_info[1]
        
        # Choose action
        action = self.choose_action(current_state)
        
        # Apply action
        simulation.fish_set_rl_outputs(self.fish_id, action[0], action[1], action[2])
        
        # Learn from reward
        reward = simulation.fish_get_last_reward(self.fish_id)
        self.total_reward += reward
        
        self.learn_from_experience(current_state, action, reward)
        
        # Store experience
        next_state = self.get_state()
        self.store_experience(current_state, action, reward, next_state)
        
        # Experience replay
        if self.frames_alive % 10 == 0:
            self.replay_experience()
        
        # Decay exploration
        self.exploration_rate = max(self.min_exploration, 
                                   self.exploration_rate * self.exploration_decay)
    
    def to_dict(self):
        """Convert brain to dictionary for saving"""
        return {
            'weights1': self.weights1,
            'weights2': self.weights2,
            'bias1': self.bias1,
            'bias2': self.bias2,
            'learning_rate': self.learning_rate,
            'exploration_rate': self.exploration_rate,
            'input_size': self.input_size,
            'hidden_size': self.hidden_size,
            'output_size': self.output_size,
            'generation': self.generation,
            'total_reward': self.total_reward,
            'reproduction_count': self.reproduction_count,
            'successful_actions': self.successful_actions,
            'failed_actions': self.failed_actions,
            'species_type': self.species_type,
            'is_predator': self.is_predator,
            'frames_alive': self.frames_alive
        }

def create_brain_for_fish(fish_id):
    """Create neural network brain for fish"""
    global fish_brains, next_fish_id
    
    # Try to find parent for inheritance
    parent_brain = None
    
    fish_info = simulation.fish_get_type_info(fish_id)
    if fish_info:
        fish_name = fish_info[0]
        is_predator = fish_info[1]
        
        # Look for successful parent of same species
        best_parent = None
        best_score = -999999
        
        for existing_fish_id, brain in fish_brains.items():
            existing_fish_info = simulation.fish_get_type_info(existing_fish_id)
            if existing_fish_info:
                if (existing_fish_info[0] == fish_name and
                    brain.total_reward > best_score and
                    brain.frames_alive > 600):
                    best_parent = brain
                    best_score = brain.total_reward
        
        parent_brain = best_parent
    
    # Create brain
    brain = PureNeuralFishBrain(fish_id, parent_brain)
    fish_brains[fish_id] = brain
    
    # Initialize reproduction tracking
    reproduction_tracking[fish_id] = 0
    
    if fish_id >= next_fish_id:
        next_fish_id = fish_id + 1
    
    return brain

def scan_for_new_fish():
    """Scan for newly spawned fish and create brains"""
    current_fish_count = simulation.fish_get_count()
    
    for fish_id in range(current_fish_count):
        if fish_id not in fish_brains:
            position = simulation.fish_get_position(fish_id)
            if position:
                create_brain_for_fish(fish_id)
                print(f"Created neural network for fish {fish_id}")

def track_reproduction_events():
    """Track reproduction events for model saving"""
    global reproduction_tracking
    
    for fish_id, brain in fish_brains.items():
        # Get current reproduction count from fish
        fish_info = simulation.fish_get_type_info(fish_id)
        if fish_info and len(fish_info) >= 4:
            current_reproductions = fish_info[3]  # defecation_count as reproduction proxy
            
            # Update reproduction count if it increased
            if current_reproductions > reproduction_tracking.get(fish_id, 0):
                reproduction_tracking[fish_id] = current_reproductions
                brain.reproduction_count = current_reproductions

def save_best_models():
    """Save the best herbivore and predator models"""
    print("\n=== SAVING BEST NEURAL NETWORK MODELS ===")
    
    best_herbivore = None
    best_predator = None
    best_herbivore_score = -999999
    best_predator_score = -999999
    
    # Find best performers
    for fish_id, brain in fish_brains.items():
        if not brain.species_type:
            continue
        
        # Calculate performance score (reproduction success + reward + longevity)
        performance_score = (brain.reproduction_count * 100 + 
                           brain.total_reward + 
                           brain.frames_alive * 0.01)
        
        if brain.is_predator:
            if performance_score > best_predator_score and brain.frames_alive > 1200:
                best_predator = brain
                best_predator_score = performance_score
        else:
            if performance_score > best_herbivore_score and brain.frames_alive > 1200:
                best_herbivore = brain
                best_herbivore_score = performance_score
    
    # Save models
    models_saved = 0
    
    if best_herbivore:
        herbivore_model = best_herbivore.to_dict()
        filename = "best_herbivore_model.json"
        
        with open(filename, 'w') as f:
            json.dump(herbivore_model, f, indent=2)
        
        print(f"✓ Saved best herbivore model: {filename}")
        print(f"  Species: {best_herbivore.species_type}")
        print(f"  Generation: {best_herbivore.generation}")
        print(f"  Reproductions: {best_herbivore.reproduction_count}")
        print(f"  Total reward: {best_herbivore.total_reward:.1f}")
        print(f"  Frames alive: {best_herbivore.frames_alive}")
        print(f"  Performance score: {best_herbivore_score:.1f}")
        models_saved += 1
    
    if best_predator:
        predator_model = best_predator.to_dict()
        filename = "best_predator_model.json"
        
        with open(filename, 'w') as f:
            json.dump(predator_model, f, indent=2)
        
        print(f"✓ Saved best predator model: {filename}")
        print(f"  Species: {best_predator.species_type}")
        print(f"  Generation: {best_predator.generation}")
        print(f"  Reproductions: {best_predator.reproduction_count}")
        print(f"  Total reward: {best_predator.total_reward:.1f}")
        print(f"  Frames alive: {best_predator.frames_alive}")
        print(f"  Performance score: {best_predator_score:.1f}")
        models_saved += 1
    
    print(f"Models saved: {models_saved}/2")
    if models_saved == 0:
        print("Warning: No suitable models found for saving (fish need >1200 frames alive)")
    
    print("=" * 80)

def signal_handler(signum, frame):
    """Handle Ctrl+C gracefully"""
    global shutdown_requested
    print(f"\nReceived signal {signum}, saving models...")
    shutdown_requested = True
    save_best_models()
    print("Shutdown complete.")
    sys.exit(0)

def print_evolution_progress():
    """Print neural network evolution progress"""
    global frame_counter
    
    print(f"\n=== NEURAL NETWORK EVOLUTION STATUS (Frame {frame_counter}) ===")
    
    active_brains = 0
    total_rewards = 0.0
    total_reproductions = 0
    herbivore_count = 0
    predator_count = 0
    
    for fish_id, brain in fish_brains.items():
        position = simulation.fish_get_position(fish_id)
        if position:
            active_brains += 1
            total_rewards += brain.total_reward
            total_reproductions += brain.reproduction_count
            
            if brain.is_predator:
                predator_count += 1
            else:
                herbivore_count += 1
    
    if active_brains > 0:
        avg_reward = total_rewards / active_brains
        avg_reproductions = total_reproductions / active_brains
        
        print(f"Active neural networks: {active_brains} ({herbivore_count} herbivores, {predator_count} predators)")
        print(f"Average reward: {avg_reward:.1f}")
        print(f"Average reproductions: {avg_reproductions:.2f}")
        print(f"Total reproduction events: {total_reproductions}")
        
        # Show top performers
        top_performers = sorted(fish_brains.items(), 
                              key=lambda x: x[1].reproduction_count + x[1].total_reward * 0.01, 
                              reverse=True)[:3]
        
        print("Top performers:")
        for fish_id, brain in top_performers[:3]:
            if simulation.fish_get_position(fish_id):
                print(f"  Fish {fish_id} ({brain.species_type}): "
                      f"Reproductions={brain.reproduction_count}, "
                      f"Reward={brain.total_reward:.1f}, "
                      f"Gen={brain.generation}")
    
    print("Updated input system: predators use inputs 0,1,3 for prey targeting")
    print("Models will be saved on shutdown (Ctrl+C)")
    print("=" * 80)

def update_fish():
    """Main update function with model saving on shutdown"""
    global frame_counter, shutdown_requested
    frame_counter += 1
    
    # Initialize on first call
    if frame_counter == 1:
        # Set up signal handler for graceful shutdown
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
        
        # Create brains for initial fish
        initial_fish_count = simulation.fish_get_count()
        for fish_id in range(initial_fish_count):
            create_brain_for_fish(fish_id)
        
        print("Neural Network Controller with Model Saving initialized!")
        print("Updated input system: predators use inputs 0,1,3 for prey targeting")
        print("Press Ctrl+C to save best models and exit gracefully")
    
    # Handle shutdown request
    if shutdown_requested:
        return
    
    # Scan for new fish
    scan_for_new_fish()
    
    # Track reproductions
    track_reproduction_events()
    
    # Update each fish
    for fish_id, brain in list(fish_brains.items()):
        position = simulation.fish_get_position(fish_id)
        if position:
            brain.update()
    
    # Evolution progress every 30 seconds
    if frame_counter % (60 * 30) == 0:
        print_evolution_progress()

if __name__ == "__main__":
    print("Neural Network Fish Controller with Model Saving loaded!")
    print("Features: Live training + Best model saving + Reproduction tracking")
    print("Updated: Predators use inputs 0,1,3 for prey targeting")