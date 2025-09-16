#!/usr/bin/env python3
"""
Improved Fish Controller with concrete survival goals
"""

import simulation
import random
import math
import numpy as np

# Global state
fish_ids = []
frame_counter = 0

class GoalOrientedFishBrain:
    def __init__(self, fish_id):
        self.fish_id = fish_id
        
        # Get vision system info from C
        vision_rays, nutrition_rays = simulation.get_vision_info()
        
        self.vision_rays = vision_rays
        self.nutrition_rays = nutrition_rays
        self.state_size = vision_rays + nutrition_rays + 3  # 27 total
        self.action_size = 2  # direction_x, direction_y
        
        # Smaller, focused network for concrete goals
        self.weights1 = np.random.randn(self.state_size, 20) * 0.2
        self.weights2 = np.random.randn(20, 12) * 0.2
        self.weights3 = np.random.randn(12, self.action_size) * 0.2
        self.bias1 = np.zeros(20)
        self.bias2 = np.zeros(12)
        self.bias3 = np.zeros(self.action_size)
        
        # Goal-oriented learning parameters
        self.learning_rate = 0.015
        self.exploration_rate = 0.4
        self.exploration_decay = 0.9995
        self.min_exploration = 0.08
        
        # Concrete goals tracking
        self.goal_weights = {
            'oxygen': 0.3,      # Keep oxygen > 0.6
            'hunger': 0.25,     # Keep hunger < 0.4
            'nutrition': 0.25,  # Find and eat food
            'survival': 0.2     # Stay alive and move efficiently
        }
        
        # Movement strategy state
        self.current_strategy = "explore"  # "explore", "seek_food", "seek_oxygen"
        self.strategy_frames = 0
        self.last_nutrition_direction = None
        
        # Performance tracking for concrete goals
        self.goal_achievements = {
            'oxygen_good': 0,
            'hunger_satisfied': 0,
            'food_found': 0,
            'successful_eating': 0,
            'exploration_effective': 0
        }
        
        # Experience memory for better learning
        self.experience_buffer = []
        self.max_experiences = 50
        
        # Learning progress indicators
        self.total_reward = 0.0
        self.learning_progress = 0.0
        self.frames_alive = 0
    
    def sigmoid(self, x):
        return 1.0 / (1.0 + np.exp(-np.clip(x, -500, 500)))
    
    def tanh(self, x):
        return np.tanh(np.clip(x, -10, 10))
    
    def relu(self, x):
        return np.maximum(0, x)
    
    def forward(self, state):
        """Forward pass through neural network"""
        # First layer
        hidden1 = np.dot(state, self.weights1) + self.bias1
        hidden1 = self.relu(hidden1)
        
        # Second layer
        hidden2 = np.dot(hidden1, self.weights2) + self.bias2
        hidden2 = self.tanh(hidden2)
        
        # Output layer
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
    
    def analyze_situation(self, state):
        """Analyze current situation and determine strategy"""
        oxygen_level = state[self.vision_rays + self.nutrition_rays]
        hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
        saturation_level = state[self.vision_rays + self.nutrition_rays + 2]
        
        # Analyze nutrition detection
        nutrition_rays = state[self.vision_rays:self.vision_rays + self.nutrition_rays]
        max_nutrition = max(nutrition_rays)
        best_nutrition_idx = np.argmax(nutrition_rays)
        
        # Determine strategy based on concrete goals
        if oxygen_level < 0.4:
            return "seek_oxygen", "CRITICAL: Low oxygen"
        elif hunger_level > 0.7:
            if max_nutrition > 0.2:
                return "seek_food", f"HUNGRY: Food detected (max: {max_nutrition:.2f})"
            else:
                return "explore", "HUNGRY: Searching for food"
        elif hunger_level > 0.4 and max_nutrition > 0.4:
            return "seek_food", f"MODERATE HUNGER: Good food detected ({max_nutrition:.2f})"
        elif oxygen_level < 0.6:
            return "seek_oxygen", "Oxygen below optimal"
        else:
            return "explore", "Maintaining territory"
    
    def choose_goal_oriented_action(self, state):
        """Choose action based on concrete survival goals"""
        strategy, reason = self.analyze_situation(state)
        
        oxygen_level = state[self.vision_rays + self.nutrition_rays]
        hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
        nutrition_rays = state[self.vision_rays:self.vision_rays + self.nutrition_rays]
        
        # Strategy-based action selection
        if strategy == "seek_oxygen":
            # Move toward areas with better oxygen (upward bias)
            if random.random() < 0.6:
                action = np.array([random.uniform(-0.3, 0.3), -0.7])  # Move up
            else:
                action = np.array([random.uniform(-0.5, 0.5), random.uniform(-0.8, -0.2)])
                
        elif strategy == "seek_food":
            # Move toward nutrition
            max_nutrition_idx = np.argmax(nutrition_rays)
            center_ray = self.nutrition_rays // 2
            
            # Calculate direction toward food
            direction_bias = (max_nutrition_idx - center_ray) / center_ray
            
            if abs(direction_bias) > 0.3:  # Strong directional signal
                action = np.array([
                    direction_bias * 0.8 + random.uniform(-0.1, 0.1),
                    random.uniform(-0.2, 0.2)
                ])
                self.last_nutrition_direction = direction_bias
            else:
                # Weak signal - explore around last known direction
                if self.last_nutrition_direction is not None:
                    action = np.array([
                        self.last_nutrition_direction * 0.5 + random.uniform(-0.3, 0.3),
                        random.uniform(-0.4, 0.4)
                    ])
                else:
                    action = np.array([random.uniform(-0.6, 0.6), random.uniform(-0.6, 0.6)])
                    
        else:  # explore
            # Intelligent exploration patterns
            if random.random() < 0.4:
                # Systematic grid search
                directions = [
                    (0.7, 0),     # right
                    (-0.7, 0),    # left
                    (0, 0.7),     # down
                    (0, -0.7),    # up
                    (0.5, 0.5),   # diagonal
                    (-0.5, -0.5), # diagonal
                ]
                action = np.array(random.choice(directions))
            else:
                # Random exploration
                action = np.array([random.uniform(-0.6, 0.6), random.uniform(-0.6, 0.6)])
        
        # Ensure reasonable magnitude
        magnitude = np.sqrt(action[0]**2 + action[1]**2)
        if magnitude > 0.9:
            action = action / magnitude * 0.9
        elif magnitude < 0.2:
            action = action / magnitude * 0.3 if magnitude > 0.01 else np.array([0.3, 0])
        
        # Track strategy
        if self.current_strategy != strategy:
            self.current_strategy = strategy
            self.strategy_frames = 0
            if self.fish_id == 0:  # Debug for first fish
                print(f"Fish {self.fish_id} strategy: {strategy} - {reason}")
        
        self.strategy_frames += 1
        
        return action
    
    def choose_action(self, state):
        """Main action selection with goal-oriented behavior"""
        # Use goal-oriented strategy most of the time
        if random.random() < (1.0 - self.exploration_rate):
            action, _, _ = self.forward(state)
            
            # Ensure reasonable movement
            magnitude = np.sqrt(action[0]**2 + action[1]**2)
            if magnitude < 0.2:
                action = self.choose_goal_oriented_action(state)
            elif magnitude > 0.9:
                action = action / magnitude * 0.8
                
        else:
            # Exploration with goal awareness
            action = self.choose_goal_oriented_action(state)
        
        return action
    
    def calculate_goal_rewards(self, state, action, reward):
        """Calculate additional rewards based on goal achievement"""
        oxygen_level = state[self.vision_rays + self.nutrition_rays]
        hunger_level = state[self.vision_rays + self.nutrition_rays + 1]
        saturation_level = state[self.vision_rays + self.nutrition_rays + 2]
        
        goal_reward = 0.0
        
        # Oxygen goal achievement
        if oxygen_level > 0.6:
            goal_reward += 0.02
            self.goal_achievements['oxygen_good'] += 1
        
        # Hunger goal achievement  
        if hunger_level < 0.4:
            goal_reward += 0.02
            self.goal_achievements['hunger_satisfied'] += 1
        
        # Food detection and eating
        nutrition_rays = state[self.vision_rays:self.vision_rays + self.nutrition_rays]
        max_nutrition = max(nutrition_rays)
        
        if max_nutrition > 0.3:
            goal_reward += 0.01
            self.goal_achievements['food_found'] += 1
        
        # Eating success (high rewards indicate eating)
        if reward > 1.0:  # Eating gives high rewards
            goal_reward += 0.1
            self.goal_achievements['successful_eating'] += 1
            if self.fish_id == 0:
                print(f"Fish {self.fish_id} successfully ate! Reward: {reward:.2f}")
        
        # Movement efficiency
        movement_magnitude = np.sqrt(action[0]**2 + action[1]**2)
        if 0.3 <= movement_magnitude <= 0.8:
            goal_reward += 0.005
            self.goal_achievements['exploration_effective'] += 1
        
        return goal_reward
    
    def learn(self, state, action, reward, next_state):
        """Enhanced learning with goal-oriented feedback"""
        if len(self.experience_buffer) == 0:
            return
        
        # Add goal-based rewards
        goal_reward = self.calculate_goal_rewards(state, action, reward)
        total_reward = reward + goal_reward
        
        # Store experience
        experience = (state.copy(), action.copy(), total_reward, next_state.copy())
        self.experience_buffer.append(experience)
        
        if len(self.experience_buffer) > self.max_experiences:
            self.experience_buffer.pop(0)
        
        # Learn from recent experiences
        if len(self.experience_buffer) >= 5:
            # Sample random experience for replay
            exp_idx = random.randint(0, len(self.experience_buffer) - 1)
            exp_state, exp_action, exp_reward, exp_next_state = self.experience_buffer[exp_idx]
            
            # Forward pass
            current_action, h1, h2 = self.forward(exp_state)
            
            # Calculate target action based on reward
            target = exp_action.copy()
            
            if exp_reward > 0.5:  # Good reward
                target = exp_action * 1.05
                self.learning_progress += 0.01
            elif exp_reward > 0.1:  # Small positive
                target = exp_action * 1.02
                self.learning_progress += 0.005
            elif exp_reward < -0.1:  # Penalty
                target = exp_action * 0.95
            
            # Clamp target
            target[0] = np.clip(target[0], -0.9, 0.9)
            target[1] = np.clip(target[1], -0.9, 0.9)
            
            # Backpropagation
            error = target - current_action
            adaptive_lr = self.learning_rate * (0.5 + 0.5 * min(1.0, self.learning_progress))
            
            # Update weights
            d_weights3 = np.outer(h2, error) * adaptive_lr
            d_bias3 = error * adaptive_lr
            
            h2_error = np.dot(error, self.weights3.T) * (1 - h2 * h2)
            h2_error = np.clip(h2_error, -1.0, 1.0)
            
            d_weights2 = np.outer(h1, h2_error) * adaptive_lr
            d_bias2 = h2_error * adaptive_lr
            
            h1_error = np.dot(h2_error, self.weights2.T) * (h1 > 0).astype(float)
            h1_error = np.clip(h1_error, -1.0, 1.0)
            
            d_weights1 = np.outer(exp_state, h1_error) * adaptive_lr
            d_bias1 = h1_error * adaptive_lr
            
            # Apply updates
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
        """Main update with goal tracking"""
        current_state = self.get_state()
        self.frames_alive += 1
        
        # Choose and apply action
        action = self.choose_action(current_state)
        simulation.fish_apply_rl_action(self.fish_id, action[0], action[1])
        
        # Get reward and learn
        reward = simulation.fish_get_last_reward(self.fish_id)
        self.total_reward += reward
        
        # Learn from experience
        if hasattr(self, 'last_state') and hasattr(self, 'last_action'):
            self.learn(self.last_state, self.last_action, reward, current_state)
        
        # Store current experience
        self.last_state = current_state.copy()
        self.last_action = action.copy()
        
        # Debug output for first fish
        if self.fish_id == 0 and hasattr(self, 'debug_counter'):
            self.debug_counter += 1
            
            if self.debug_counter % 300 == 0:  # Every 5 seconds
                oxygen = current_state[self.vision_rays + self.nutrition_rays]
                hunger = current_state[self.vision_rays + self.nutrition_rays + 1]
                saturation = current_state[self.vision_rays + self.nutrition_rays + 2]
                max_nutrition = max(current_state[self.vision_rays:self.vision_rays + self.nutrition_rays])
                
                print(f"Fish {self.fish_id}: O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                      f"Stomach={saturation:.2f}, MaxNutr={max_nutrition:.3f}, "
                      f"Strategy={self.current_strategy}, Reward={reward:.3f}, "
                      f"Explore={self.exploration_rate:.3f}")
        elif self.fish_id == 0:
            self.debug_counter = 0

# Global fish brains
fish_brains = {}

def initialize_fish():
    """Create goal-oriented fish brains"""
    global fish_ids, fish_brains
    
    world_left, world_top, world_right, world_bottom = simulation.get_world_bounds()
    fish_type_count = simulation.fish_get_type_count()
    
    if fish_type_count == 0:
        print("No fish types available")
        return
    
    vision_rays, nutrition_rays = simulation.get_vision_info()
    print(f"Goal-Oriented Fish Controller: {vision_rays} vision rays, {nutrition_rays} nutrition rays")
    
    existing_fish_count = simulation.fish_get_count()
    print(f"Creating goal-oriented brains for {existing_fish_count} fish")
    
    for i in range(existing_fish_count):
        fish_brains[i] = GoalOrientedFishBrain(i)
        fish_ids.append(i)
        print(f"Created goal-oriented brain for fish {i}")

def print_goal_progress():
    """Print goal achievement progress"""
    print(f"\n=== GOAL-ORIENTED FISH PROGRESS (Frame {frame_counter}) ===")
    
    total_frames = 0
    total_goals = {key: 0 for key in fish_brains[0].goal_achievements.keys() if 0 in fish_brains}
    
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            brain = fish_brains[fish_id]
            total_frames += brain.frames_alive
            
            for goal, count in brain.goal_achievements.items():
                total_goals[goal] += count
            
            oxygen = simulation.fish_get_oxygen_level(fish_id)
            hunger = simulation.fish_get_hunger_level(fish_id)
            saturation = simulation.fish_get_saturation_level(fish_id)
            
            print(f"Fish {fish_id}: O2={oxygen:.2f}, Hunger={hunger:.2f}, "
                  f"Stomach={saturation:.2f}, Strategy={brain.current_strategy}, "
                  f"Progress={brain.learning_progress:.2f}, Alive={brain.frames_alive}")
    
    # Calculate success rates
    if total_frames > 0:
        print(f"\n=== GOAL ACHIEVEMENT RATES ===")
        for goal, total_count in total_goals.items():
            rate = (total_count / total_frames) * 100
            print(f"{goal}: {rate:.2f}% ({total_count}/{total_frames})")
        
        print("=" * 60)

def update_fish():
    """Main update with goal tracking"""
    global frame_counter
    frame_counter += 1
    
    # Initialize fish on first call
    if frame_counter == 1:
        initialize_fish()
        print("Goal-Oriented Fish Controller Initialized!")
        print("Concrete Goals: Oxygen>0.6, Hunger<0.4, Find Food, Survive")
    
    # Update each fish brain
    for fish_id in fish_ids:
        if fish_id in fish_brains:
            fish_brains[fish_id].update()
    
    # Print progress every 30 seconds
    if frame_counter % (60 * 30) == 0:
        print_goal_progress()

if __name__ == "__main__":
    print("Goal-Oriented Fish Controller loaded!")
    print("Concrete survival goals: Oxygen, Hunger, Nutrition, Survival")