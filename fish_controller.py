#!/usr/bin/env python3
"""
Enhanced RL Fish Controller with Predator-Prey System
Extended Inputs: plant_vector_x, plant_vector_y, oxygen_level, plant_distance, 
                 foreign_fish_vector_x, foreign_fish_vector_y, danger_level
Outputs: turn_direction, movement_strength, eat_command
"""

import simulation
import random
import math

# Global state
fish_ids = []
frame_counter = 0

class EnhancedFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        print(f"Creating enhanced brain for fish {fish_id} with predator-prey awareness")
        
        # Enhanced neural network for 7 inputs
        self.input_size = 7
        self.hidden_size = 16
        self.output_size = 3
        
        # Initialize weights
        self.weights1 = [[random.uniform(-0.3, 0.3) for _ in range(self.hidden_size)] for _ in range(self.input_size)]
        self.weights2 = [[random.uniform(-0.3, 0.3) for _ in range(self.output_size)] for _ in range(self.hidden_size)]
        self.bias1 = [0.0] * self.hidden_size
        self.bias2 = [0.0] * self.output_size
        
        # Learning parameters
        self.learning_rate = 0.02
        self.exploration_rate = 0.7
        self.exploration_decay = 0.999
        self.min_exploration = 0.1
        
        # Performance tracking
        self.successful_eats = 0
        self.failed_eats = 0
        self.total_reward = 0.0
        self.frames_alive = 0
        self.predator_encounters = 0
        self.successful_escapes = 0
        
        # Memory for smoother behavior
        self.last_action = [0.0, 0.6, 0.1]
        self.action_persistence = 0
        self.panic_mode = False
        self.panic_cooldown = 0
    
    def sigmoid(self, x):
        return 1.0 / (1.0 + math.exp(-max(-500, min(500, x))))
    
    def tanh(self, x):
        return math.tanh(max(-10, min(10, x)))
    
    def relu(self, x):
        return max(0, x)
    
    def forward(self, inputs):
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
        turn_direction = self.tanh(outputs[0])
        movement_strength = self.sigmoid(outputs[1])
        eat_command = self.sigmoid(outputs[2])
        
        return [turn_direction, movement_strength, eat_command], hidden
    
    def get_state(self):
        inputs = simulation.fish_get_rl_inputs(self.fish_id)
        if inputs is None:
            return [0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0]
        return list(inputs)
    
    def choose_action_intelligent(self, state):
        """Enhanced rule-based behavior with predator-prey dynamics"""
        plant_vector_x, plant_vector_y, oxygen_level, plant_distance, \
        foreign_fish_vector_x, foreign_fish_vector_y, danger_level = state
        
        plant_magnitude = math.sqrt(plant_vector_x**2 + plant_vector_y**2)
        plant_detected = plant_magnitude > 0.1
        
        foreign_fish_magnitude = math.sqrt(foreign_fish_vector_x**2 + foreign_fish_vector_y**2)
        fish_detected = foreign_fish_magnitude > 0.1
        
        # Update panic mode
        if self.panic_cooldown > 0:
            self.panic_cooldown -= 1
        
        # PREDATOR-PREY LOGIC
        if fish_detected and abs(danger_level) > 0.1:
            if danger_level < -0.2:  # Dangerous predator nearby
                self.predator_encounters += 1
                self.panic_mode = True
                self.panic_cooldown = 60  # Panic for 2 seconds
                
                # FLEE: Turn away from predator and move fast
                flee_angle = math.atan2(-foreign_fish_vector_y, -foreign_fish_vector_x)
                turn_direction = math.tanh(flee_angle)
                movement_strength = 0.9  # Maximum speed
                eat_command = 0.0  # Don't eat while fleeing
                
                if random.random() < 0.3:  # Add evasive maneuvers
                    turn_direction += random.uniform(-0.5, 0.5)
                    turn_direction = max(-1.0, min(1.0, turn_direction))
                
                return [turn_direction, movement_strength, eat_command]
            
            elif danger_level > 0.2:  # Weaker prey nearby (for predators)
                # HUNT: Move toward prey
                hunt_angle = math.atan2(foreign_fish_vector_y, foreign_fish_vector_x)
                turn_direction = math.tanh(hunt_angle)
                movement_strength = 0.8
                
                # Close enough to attack?
                fish_distance = 1.0 - (foreign_fish_magnitude / 2.0)  # Rough estimate
                if fish_distance < 0.3:
                    eat_command = 0.9
                    movement_strength = 0.6  # Slow down for precision
                else:
                    eat_command = 0.1
                
                return [turn_direction, movement_strength, eat_command]
        
        # NORMAL BEHAVIOR (no immediate predator-prey interaction)
        if self.panic_mode:
            # Still in panic mode - keep moving
            if random.random() < 0.4:
                turn_direction = random.uniform(-0.8, 0.8)
            else:
                turn_direction = random.uniform(-0.3, 0.3)
            movement_strength = 0.8
            eat_command = 0.1
            
            if self.panic_cooldown <= 0:
                self.panic_mode = False
                self.successful_escapes += 1
            
            return [turn_direction, movement_strength, eat_command]
        
        # PLANT SEEKING BEHAVIOR (for herbivores)
        if plant_detected and plant_distance < 0.8:
            plant_angle = math.atan2(plant_vector_y, plant_vector_x)
            turn_direction = math.tanh(plant_angle)
            
            if plant_distance < 0.2:  # Very close
                movement_strength = 0.3
                eat_command = 0.8
            elif plant_distance < 0.5:  # Moderately close
                movement_strength = 0.6
                eat_command = 0.6
            else:  # Far but detected
                movement_strength = 0.8
                eat_command = 0.2
        else:
            # EXPLORATION BEHAVIOR
            if random.random() < 0.3:
                turn_directions = [-0.8, -0.4, 0.0, 0.4, 0.8]
                turn_direction = random.choice(turn_directions)
            else:
                turn_direction = random.uniform(-0.4, 0.4)
            
            movement_strength = 0.7  # Keep moving to avoid camping
            eat_command = 0.1
        
        # Oxygen-based adjustments
        if oxygen_level < 0.3:
            movement_strength = min(movement_strength + 0.2, 1.0)
            if not plant_detected:
                turn_direction += random.uniform(-0.2, 0.2)
        elif oxygen_level > 0.8:
            movement_strength *= 0.85
        
        return [turn_direction, movement_strength, eat_command]
    
    def choose_action(self, state):
        """Enhanced action selection with persistence"""
        if self.action_persistence > 0:
            self.action_persistence -= 1
            # Slight variation for more natural movement
            action = self.last_action[:]
            if random.random() < 0.2:
                action[0] += random.uniform(-0.1, 0.1)
                action[1] += random.uniform(-0.05, 0.05)
            return action
        
        if random.random() < self.exploration_rate:
            # Exploration using intelligent rules
            action = self.choose_action_intelligent(state)
            self.action_persistence = random.randint(5, 12)  # Shorter persistence for reactivity
        else:
            # Exploitation using neural network
            action, _ = self.forward(state)
            
            # Add small randomness for exploration
            if random.random() < 0.1:
                action[0] += random.uniform(-0.08, 0.08)
                action[1] += random.uniform(-0.03, 0.03)
                action[2] += random.uniform(-0.03, 0.03)
        
        # Clamp values
        action[0] = max(-1.0, min(1.0, action[0]))
        action[1] = max(0.0, min(1.0, action[1]))
        action[2] = max(0.0, min(1.0, action[2]))
        
        self.last_action = action[:]
        return action
    
    def learn(self, state, action, reward):
        """Enhanced learning with predator-prey adaptations"""
        if reward > 0.5:
            # Very good reward
            if action[2] > 0.5:
                self.successful_eats += 1
            
            # Reinforce successful behavior
            outputs, hidden = self.forward(state)
            for i in range(self.output_size):
                error = action[i] - outputs[i]
                self.bias2[i] += error * self.learning_rate * 0.15
                
                for j in range(self.hidden_size):
                    self.weights2[j][i] += hidden[j] * error * self.learning_rate * 0.15
        
        elif reward < -0.5:
            # Very bad reward
            if action[2] > 0.5:
                self.failed_eats += 1
        
        # Decay exploration
        self.exploration_rate = max(self.min_exploration, 
                                   self.exploration_rate * self.exploration_decay)
    
    def update(self):
        """Enhanced update with predator-prey tracking"""
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
            if self.debug_counter % 180 == 0:
                plant_distance = current_state[3]
                oxygen = current_state[2]
                danger_level = current_state[6]
                eating_mode = simulation.fish_is_eating(self.fish_id)
                stomach = simulation.fish_get_stomach_contents(self.fish_id)
                
                print(f"Fish {self.fish_id}: PlantDist={plant_distance:.2f}, O2={oxygen:.2f}, "
                      f"Danger={danger_level:.2f}, Action=({action[0]:.2f},{action[1]:.2f},{action[2]:.2f}), "
                      f"Eating={eating_mode}, Stomach={stomach:.2f}, Reward={reward:.3f}, "
                      f"Escapes={self.successful_escapes}, Encounters={self.predator_encounters}")
        elif self.fish_id == 0:
            self.debug_counter = 0

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create enhanced fish brains with predator-prey awareness"""
    global fish_ids, fish_brains
    
    existing_fish_count = simulation.fish_get_count()
    print(f"Creating enhanced brains for {existing_fish_count} fish with predator-prey system")
    
    for i in range(existing_fish_count):
        fish_brains[i] = EnhancedFishBrain(i)
        fish_ids.append(i)

def print_learning_progress():
    """Enhanced progress reporting with predator-prey stats"""
    print(f"\n=== ENHANCED FISH STATUS (Frame {frame_counter}) ===")
    
    active_brains = 0
    total_rewards = 0.0
    total_successful_eats = 0
    total_failed_eats = 0
    total_exploration = 0.0
    total_predator_encounters = 0
    total_successful_escapes = 0
    
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
                total_predator_encounters += brain.predator_encounters
                total_successful_escapes += brain.successful_escapes
                
                stomach = simulation.fish_get_stomach_contents(fish_id)
                eating = simulation.fish_is_eating(fish_id)
                
                print(f"Fish {fish_id}: Stomach={stomach:.2f}, Eating={eating}, "
                      f"Eat={brain.successful_eats}/{brain.failed_eats}, "
                      f"Predator={brain.predator_encounters}, Escapes={brain.successful_escapes}, "
                      f"Reward={brain.total_reward:.1f}, Explore={brain.exploration_rate:.3f}")
    
    if active_brains > 0:
        avg_reward = total_rewards / active_brains
        avg_exploration = total_exploration / active_brains
        eat_attempts = total_successful_eats + total_failed_eats
        success_rate = 0.0
        if eat_attempts > 0:
            success_rate = total_successful_eats / eat_attempts
        
        escape_rate = 0.0
        if total_predator_encounters > 0:
            escape_rate = total_successful_escapes / total_predator_encounters
        
        print(f"\n=== ENHANCED LEARNING SUMMARY ===")
        print(f"Active fish: {active_brains}")
        print(f"Average reward: {avg_reward:.1f}")
        print(f"Average exploration: {avg_exploration:.3f}")
        print(f"Eating success rate: {success_rate:.2f} ({total_successful_eats}/{eat_attempts})")
        print(f"Predator encounters: {total_predator_encounters}")
        print(f"Escape success rate: {escape_rate:.2f} ({total_successful_escapes}/{total_predator_encounters})")
        
        # Learning status with predator-prey awareness
        if avg_exploration < 0.15 and success_rate > 0.3 and escape_rate > 0.6:
            print("🟢 LEARNING: Fish are mastering predator-prey dynamics!")
        elif escape_rate > 0.5:
            print("🟡 LEARNING: Fish are learning to survive predators")
        elif avg_exploration > 0.5:
            print("🟡 LEARNING: Still exploring predator-prey strategies")
        else:
            print("🟡 LEARNING: Adapting to ecosystem dynamics")
        
        # Nutrition balance
        fish_consumed, fish_defecated, env_added, env_depleted = simulation.get_nutrition_balance()
        print(f"Nutrition: Consumed={fish_consumed:.2f}, Defecated={fish_defecated:.2f}")
    
    print("=" * 80)

def update_fish():
    """Enhanced update function with predator-prey system"""
    global frame_counter
    frame_counter += 1
    
    # Initialize on first call
    if frame_counter == 1:
        initialize_fish()
        print("Enhanced Fish Controller Initialized!")
        print("Features: Predator-Prey Dynamics + Rule-based + Neural Network")
        print("Extended inputs: plant detection + foreign fish detection + danger levels")
        print("Behaviors: Hunt, flee, forage, reproduce")
    
    # Update each fish
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Progress every 20 seconds with enhanced stats
    if frame_counter % (60 * 20) == 0:
        print_learning_progress()

if __name__ == "__main__":
    print("Enhanced Fish Controller with Predator-Prey System loaded!")
    print("Features: 7-input RL system with hunt/flee behaviors")