#!/usr/bin/env python3
"""
Fish Controller with configurable population
"""

import simulation
import random
import math
import numpy as np

# Global state
fish_ids = []
frame_counter = 0

class CalmFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        
        # Get vision system info from C
        vision_rays, nutrition_rays = simulation.get_vision_info()
        
        self.vision_rays = vision_rays
        self.nutrition_rays = nutrition_rays
        self.state_size = vision_rays + nutrition_rays + 3  # 27 total
        self.action_size = 2  # direction_x, direction_y
        
        # Smaller, more focused network for better learning
        self.weights1 = np.random.randn(self.state_size, 16) * 0.3
        self.weights2 = np.random.randn(16, 8) * 0.3
        self.weights3 = np.random.randn(8, self.action_size) * 0.3
        self.bias1 = np.zeros(16)
        self.bias2 = np.zeros(8)
        self.bias3 = np.zeros(self.action_size)
        
        # Calm learning parameters
        self.learning_rate = 0.01
        self.exploration_rate = 0.3
        self.exploration_decay = 0.999
        self.min_exploration = 0.05
        
        # Movement persistence for calmer behavior
        self.current_direction_x = 0.0
        self.current_direction_y = 0.0
        self.direction_change_cooldown = 0
        self.preferred_direction_duration = 30
        
        # Enhanced experience tracking
        self.last_state = None
        self.last_action = None
        self.total_reward = 0.0
        self.movement_history = []
        self.learning_progress = 0.0
        
    def sigmoid(self, x):
        return 1.0 / (1.0 + np.exp(-np.clip(x, -500, 500)))
    
    def tanh(self, x):
        return np.tanh(np.clip(x, -10, 10))
    
    def relu(self, x):
        return np.maximum(0, x)
    
    def forward(self, state):
        """Smaller, more focused network for stable learning"""
        # First layer: smaller for faster learning
        hidden1 = np.dot(state, self.weights1) + self.bias1
        hidden1 = self.relu(hidden1)
        
        # Second layer: focused processing
        hidden2 = np.dot(hidden1, self.weights2) + self.bias2
        hidden2 = self.tanh(hidden2)
        
        # Output layer: smooth direction control
        output = np.dot(hidden2, self.weights3) + self.bias3
        direction_x = self.tanh(output[0])
        direction_y = self.tanh(output[1])
        
        return np.array([direction_x, direction_y]), hidden1, hidden2
    
    def get_state(self):
        """Get enhanced state vector"""
        state = np.zeros(self.state_size)
        
        # Vision rays
        for i in range(self.vision_rays):
            state[i] = simulation.fish_get_vision_ray(self.fish_id, i)
        
        # Chemoreceptor rays
        for i in range(self.nutrition_rays):
            state[self.vision_rays + i] = simulation.fish_get_nutrition_ray(self.fish_id, i)
        
        # Core state variables
        base_idx = self.vision_rays + self.nutrition_rays
        state[base_idx] = simulation.fish_get_oxygen_level(self.fish_id)
        state[base_idx + 1] = simulation.fish_get_hunger_level(self.fish_id)
        state[base_idx + 2] = simulation.fish_get_saturation_level(self.fish_id)
        
        return state
    
    def choose_action(self, state):
        """Calm action selection with learning progression"""
        # Reduce direction change frequency for calmer movement
        if self.direction_change_cooldown > 0:
            self.direction_change_cooldown -= 1
            # Continue in current direction with small variations
            variation_x = random.uniform(-0.1, 0.1)
            variation_y = random.uniform(-0.1, 0.1)
            action = np.array([
                np.clip(self.current_direction_x + variation_x, -1.0, 1.0),
                np.clip(self.current_direction_y + variation_y, -1.0, 1.0)
            ])
            return action
        
        # Lower exploration rate for calmer behavior
        if random.random() < self.exploration_rate:
            # Calmer exploration with guided behavior
            oxygen_level = state[self.vision_rays + self.nutrition_rays]
            hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
            
            # Calculate nutrition detection for guidance
            nutrition_rays = state[self.vision_rays:self.vision_rays + self.nutrition_rays]
            max_nutrition_ray = max(nutrition_rays)
            best_nutrition_ray_idx = np.argmax(nutrition_rays)
            
            # Smart exploration based on needs
            if oxygen_level < 0.3:
                # Low oxygen: gentle movement toward better areas
                direction_x = random.uniform(-0.6, 0.6)
                direction_y = random.uniform(-0.6, 0.6)
            elif hunger_level > 0.6 and max_nutrition_ray > 0.3:
                # Hungry with food detected: move toward nutrition
                center_ray = self.nutrition_rays // 2
                direction_bias_x = (best_nutrition_ray_idx - center_ray) / center_ray * 0.6
                
                direction_x = direction_bias_x + random.uniform(-0.2, 0.2)
                direction_y = random.uniform(-0.3, 0.3)
                
                direction_x = np.clip(direction_x, -0.8, 0.8)
                direction_y = np.clip(direction_y, -0.8, 0.8)
            else:
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
            
            # Set cooldown for calmer behavior
            self.direction_change_cooldown = random.randint(20, 40)
            
        else:
            # Exploit: use network
            action, _, _ = self.forward(state)
            
            # Ensure reasonable movement strength
            magnitude = np.sqrt(action[0]**2 + action[1]**2)
            if magnitude < 0.2:
                if magnitude > 0.01:
                    action = action / magnitude * 0.3
                else:
                    # Very gentle random direction if action is near zero
                    angle = random.random() * 2 * math.pi
                    action = np.array([math.cos(angle) * 0.3, math.sin(angle) * 0.3])
            elif magnitude > 0.8:
                # Cap maximum movement for calmer behavior
                action = action / magnitude * 0.8
        
        # Store current direction
        self.current_direction_x = action[0]
        self.current_direction_y = action[1]
        
        return action
    
    def learn(self, state, action, reward, next_state):
        """Enhanced learning with better stability"""
        if self.last_state is None:
            return
        
        # Calculate current network output
        current_action, h1, h2 = self.forward(state)
        
        # Calculate target based on reward feedback
        target = action.copy()
        
        # More stable reward-based learning
        reward_magnitude = abs(reward)
        
        if reward > 0.5:  # Good reward (eating)
            # Strongly reinforce this action
            target = action * 1.1
            self.learning_progress += 0.02
        elif reward > 0.05:  # Small positive reward
            # Slightly reinforce
            target = action * 1.02
            self.learning_progress += 0.005
        elif reward < -0.05:  # Penalty
            # Analyze what went wrong, but don't overreact
            oxygen_level = state[self.vision_rays + self.nutrition_rays]
            hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
            
            if oxygen_level < 0.4:  # Low oxygen
                # Try slightly different direction
                target[0] = action[0] * 0.8 + random.uniform(-0.1, 0.1)
                target[1] = action[1] * 0.8 + random.uniform(-0.1, 0.1)
            elif hunger_level > 0.7:  # Very hungry
                # Encourage more exploration but gently
                target = action * 1.05
            else:
                # General penalty - small adjustment
                target = action * 0.95
        
        # Improved chemoreceptor learning
        nutrition_rays = state[self.vision_rays:self.vision_rays + self.nutrition_rays]
        next_nutrition_rays = next_state[self.vision_rays:self.vision_rays + self.nutrition_rays]
        
        nutrition_improvement = sum(next_nutrition_rays) - sum(nutrition_rays)
        if nutrition_improvement > 0.05 and reward >= 0:
            # Good nutrition gradient following
            target = action * 1.02
            self.learning_progress += 0.01
        
        # Clamp target to reasonable range
        target[0] = np.clip(target[0], -0.8, 0.8)
        target[1] = np.clip(target[1], -0.8, 0.8)
        
        # Backpropagation with stability improvements
        error = target - current_action
        
        # Scale learning rate based on progress
        adaptive_lr = self.learning_rate * (0.5 + 0.5 * min(1.0, self.learning_progress))
        
        # Output layer gradients
        d_weights3 = np.outer(h2, error) * adaptive_lr
        d_bias3 = error * adaptive_lr
        
        # Hidden layer gradients with gradient clipping
        h2_error = np.dot(error, self.weights3.T)
        h2_error = h2_error * (1 - h2 * h2)  # tanh derivative
        h2_error = np.clip(h2_error, -1.0, 1.0)  # Gradient clipping
        
        d_weights2 = np.outer(h1, h2_error) * adaptive_lr
        d_bias2 = h2_error * adaptive_lr
        
        # First hidden layer gradients
        h1_error = np.dot(h2_error, self.weights2.T)
        h1_error = h1_error * (h1 > 0).astype(float)  # ReLU derivative
        h1_error = np.clip(h1_error, -1.0, 1.0)  # Gradient clipping
        
        d_weights1 = np.outer(state, h1_error) * adaptive_lr
        d_bias1 = h1_error * adaptive_lr
        
        # Update weights with smaller steps for stability
        self.weights3 += d_weights3
        self.bias3 += d_bias3
        self.weights2 += d_weights2
        self.bias2 += d_bias2
        self.weights1 += d_weights1
        self.bias1 += d_bias1
        
        # Gradual exploration decay
        self.exploration_rate = max(self.min_exploration, 
                                   self.exploration_rate * self.exploration_decay)
    
    def update(self):
        """Enhanced update with calmer behavior tracking"""
        current_state = self.get_state()
        
        # Track movement patterns for learning assessment
        position = simulation.fish_get_position(self.fish_id)
        if position:
            self.movement_history.append(position)
            if len(self.movement_history) > 120:
                self.movement_history.pop(0)
        
        # Choose and apply action
        action = self.choose_action(current_state)
        simulation.fish_apply_rl_action(self.fish_id, action[0], action[1])
        
        # Get reward and learn
        reward = simulation.fish_get_last_reward(self.fish_id)
        self.total_reward += reward
        
        # Learn from experience
        if self.last_state is not None and self.last_action is not None:
            self.learn(self.last_state, self.last_action, reward, current_state)
        
        # Store current experience
        self.last_state = current_state.copy()
        self.last_action = action.copy()
        
        # Debug output for first fish with learning progress
        if self.fish_id == 0 and simulation.fish_get_count() > 0:
            if hasattr(self, 'debug_counter'):
                self.debug_counter += 1
            else:
                self.debug_counter = 0
                
            if self.debug_counter % 180 == 0:  # Every 3 seconds
                oxygen = current_state[self.vision_rays + self.nutrition_rays]
                hunger = current_state[self.vision_rays + self.nutrition_rays + 1]
                max_nutrition = max(current_state[self.vision_rays:self.vision_rays + self.nutrition_rays])
                
                print(f"Fish {self.fish_id}: O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                      f"MaxNutr={max_nutrition:.3f}, Action=({action[0]:.2f},{action[1]:.2f}), "
                      f"Reward={reward:.3f}, Explore={self.exploration_rate:.3f}, "
                      f"Progress={self.learning_progress:.2f}")

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create fish population based on configurable count"""
    global fish_ids, fish_brains
    
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")
        return
    
    vision_rays, nutrition_rays = simulation.get_vision_info()
    print(f"Calm Fish Controller: {vision_rays} vision rays, {nutrition_rays} nutrition rays")
    print(f"World size: {world_right-world_left:.0f}x{world_bottom-world_top:.0f}")
    
    # Note: Fish are already spawned by main.c based on INITIAL_FISH_COUNT
    # This just creates the brains for existing fish
    existing_fish_count = simulation.fish_get_count()
    print(f"Creating brains for {existing_fish_count} existing fish")
    
    for i in range(existing_fish_count):
        fish_brains[i] = CalmFishBrain(i)
        fish_ids.append(i)
        print(f"Created brain for fish {i}")

def print_learning_progress():
    """Print learning progress and behavior analysis"""
    print(f"\n=== CALM FISH LEARNING STATUS (Frame {frame_counter}) ===")
    
    total_oxygen = 0.0
    total_hunger = 0.0
    total_rewards = 0.0
    total_movement = 0.0
    total_exploration = 0.0
    total_progress = 0.0
    learning_fish = []
    calm_fish = []
    
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            brain = fish_brains[fish_id]
            energy = simulation.fish_get_energy(fish_id)
            
            if energy is not None:
                oxygen = simulation.fish_get_oxygen_level(fish_id)
                hunger = simulation.fish_get_hunger_level(fish_id)
                last_reward = simulation.fish_get_last_reward(fish_id)
                
                velocity = simulation.fish_get_velocity(fish_id)
                current_speed = 0.0
                if velocity:
                    vx, vy = velocity
                    current_speed = math.sqrt(vx*vx + vy*vy)
                
                total_oxygen += oxygen
                total_hunger += hunger
                total_rewards += brain.total_reward
                total_movement += current_speed
                total_exploration += brain.exploration_rate
                total_progress += brain.learning_progress
                
                # Assess individual fish
                if brain.learning_progress > 0.5 and brain.exploration_rate < 0.15:
                    learning_fish.append((fish_id, brain.learning_progress, current_speed))
                
                if current_speed < 4.0 and brain.exploration_rate < 0.2:
                    calm_fish.append((fish_id, current_speed, brain.exploration_rate))
                
                print(f"Fish {fish_id}: Energy={energy:.2f}, Speed={current_speed:.2f}, "
                      f"O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                      f"Explore={brain.exploration_rate:.3f}, Progress={brain.learning_progress:.2f}, "
                      f"TotalReward={brain.total_reward:.2f}")
    
    # Summary statistics
    active_count = len(fish_ids)
    if active_count > 0:
        avg_oxygen = total_oxygen / active_count
        avg_hunger = total_hunger / active_count
        avg_reward = total_rewards / active_count
        avg_movement = total_movement / active_count
        avg_exploration = total_exploration / active_count
        avg_progress = total_progress / active_count
        
        print(f"\n=== LEARNING SUMMARY ===")
        print(f"Active fish: {active_count}")
        print(f"Average movement speed: {avg_movement:.2f} (target: 3-6)")
        print(f"Average exploration rate: {avg_exploration:.3f} (should decrease over time)")
        print(f"Average learning progress: {avg_progress:.2f} (should increase over time)")
        print(f"Average oxygen: {avg_oxygen:.2f}")
        print(f"Average hunger: {avg_hunger:.2f}")
        
        # Learning assessment
        if avg_exploration < 0.15 and avg_progress > 0.3:
            print("🟢 LEARNING: Fish are learning well and becoming less random")
        elif avg_exploration > 0.25:
            print("🟡 LEARNING: Fish still exploring, early learning phase")
        else:
            print("🔴 LEARNING: Fish may be stuck, check reward system")
        
        # Movement assessment
        if 3.0 <= avg_movement <= 6.0:
            print("🟢 MOVEMENT: Fish swimming at calm, natural speed")
        elif avg_movement > 8.0:
            print("🔴 MOVEMENT: Fish too fast/frantic")
        else:
            print("🟡 MOVEMENT: Fish moving slowly")
        
        if learning_fish:
            print(f"🎓 Advanced learners: {learning_fish}")
        
        if calm_fish:
            print(f"😌 Calm swimmers: {calm_fish}")
        
        print("=" * 60)

def update_fish():
    """Enhanced update with learning tracking"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
        print("Calm Learning Fish Controller Initialized!")
        print("Features: Reduced exploration, movement persistence, progressive learning")
        print("Fish should become calmer and smarter over time!")
    
    # Update each fish brain
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Print progress every 20 seconds to observe learning
    if frame_counter % (60 * 20) == 0:
        print_learning_progress()

if __name__ == "__main__":
    print("Calm Learning Fish Controller loaded!")