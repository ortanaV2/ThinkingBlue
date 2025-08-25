import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Circle
import random
import math
from collections import deque
import time

class AlgaeEnvironment:
    def __init__(self, width=100, height=100, num_algae=8, max_speed=3):
        self.width = width
        self.height = height
        self.num_algae = num_algae
        self.max_speed = max_speed
        self.reset()
        
    def reset(self):
        # Random agent position
        self.agent_x = random.uniform(10, self.width-10)
        self.agent_y = random.uniform(10, self.height-10)
        
        # Random algae positions
        self.algae = []
        for _ in range(self.num_algae):
            x = random.uniform(10, self.width-10)
            y = random.uniform(10, self.height-10)
            self.algae.append([x, y])
            
        self.score = 0
        self.steps = 0
        self.total_reward = 0
        return self.get_state()
    
    def get_nearest_algae_info(self):
        if not self.algae:
            return self.width + self.height, None, 0, 0
        
        min_dist = float('inf')
        nearest_algae = None
        for i, (algae_x, algae_y) in enumerate(self.algae):
            dist = math.sqrt((self.agent_x - algae_x)**2 + (self.agent_y - algae_y)**2)
            if dist < min_dist:
                min_dist = dist
                nearest_algae = i
        
        if nearest_algae is not None:
            algae_x, algae_y = self.algae[nearest_algae]
            # Direction vector to nearest algae (normalized)
            dx = algae_x - self.agent_x
            dy = algae_y - self.agent_y
            return min_dist, nearest_algae, dx/min_dist, dy/min_dist
        
        return min_dist, nearest_algae, 0, 0
    
    def get_state(self):
        # Multi-dimensional state: [distance_bin, direction_x_bin, direction_y_bin]
        distance, _, dx, dy = self.get_nearest_algae_info()
        
        # Distance discretization (0-7: closer is better)
        max_dist = math.sqrt(self.width**2 + self.height**2)
        dist_norm = min(distance / (max_dist * 0.7), 1.0)  # Scale down for better bins
        dist_bin = int(dist_norm * 7)
        
        # Direction discretization (-1 to 1 mapped to 0-4)
        dx_bin = int((dx + 1) * 2)  # -1->0, 0->2, 1->4
        dy_bin = int((dy + 1) * 2)
        
        dx_bin = max(0, min(4, dx_bin))
        dy_bin = max(0, min(4, dy_bin))
        
        # Combined state index
        state = dist_bin * 25 + dx_bin * 5 + dy_bin  # 8*5*5 = 200 states
        return min(state, 199)
    
    def step(self, action):
        # 8 directions with higher speed
        directions = [
            (0, self.max_speed),                              # North
            (self.max_speed*0.707, self.max_speed*0.707),     # Northeast  
            (self.max_speed, 0),                              # East
            (self.max_speed*0.707, -self.max_speed*0.707),    # Southeast
            (0, -self.max_speed),                             # South
            (-self.max_speed*0.707, -self.max_speed*0.707),   # Southwest
            (-self.max_speed, 0),                             # West
            (-self.max_speed*0.707, self.max_speed*0.707)     # Northwest
        ]
        
        dx, dy = directions[action]
        
        # Store previous distance for reward shaping
        prev_distance, _, _, _ = self.get_nearest_algae_info()
        
        # Move agent with boundaries
        self.agent_x = max(3, min(self.width-3, self.agent_x + dx))
        self.agent_y = max(3, min(self.height-3, self.agent_y + dy))
        
        # Check for algae consumption
        reward = 0
        eaten_algae = []
        
        for i, (algae_x, algae_y) in enumerate(self.algae):
            distance = math.sqrt((self.agent_x - algae_x)**2 + (self.agent_y - algae_y)**2)
            if distance < 5:  # Larger collision radius
                reward += 100  # Large reward for eating algae
                eaten_algae.append(i)
                self.score += 1
        
        # Remove eaten algae
        for i in sorted(eaten_algae, reverse=True):
            del self.algae[i]
        
        # Reward shaping: encourage moving towards nearest algae
        if not eaten_algae and self.algae:  # Only if no algae was eaten
            new_distance, _, _, _ = self.get_nearest_algae_info()
            distance_improvement = prev_distance - new_distance
            reward += distance_improvement * 0.5  # Reward getting closer
            reward -= 0.1  # Small penalty for each step
        
        self.steps += 1
        self.total_reward += reward
        
        # Episode ends when all algae eaten or too many steps
        done = len(self.algae) == 0 or self.steps > 300
        
        return self.get_state(), reward, done

class ImprovedQLearningAgent:
    def __init__(self, state_size=200, action_size=8, learning_rate=0.3, 
                 discount_factor=0.99, epsilon=0.9, epsilon_decay=0.996, 
                 epsilon_min=0.05):
        self.state_size = state_size
        self.action_size = action_size
        self.learning_rate = learning_rate
        self.discount_factor = discount_factor
        self.epsilon = epsilon
        self.epsilon_decay = epsilon_decay
        self.epsilon_min = epsilon_min
        
        # Initialize Q-table with small random values to break symmetry
        self.q_table = np.random.uniform(-0.01, 0.01, (state_size, action_size))
        
    def choose_action(self, state):
        if np.random.random() <= self.epsilon:
            return random.randrange(self.action_size)
        return np.argmax(self.q_table[state])
    
    def learn(self, state, action, reward, next_state, done):
        # Q-learning update with improved parameters
        target = reward
        if not done:
            target = reward + self.discount_factor * np.max(self.q_table[next_state])
        
        current_q = self.q_table[state, action]
        self.q_table[state, action] += self.learning_rate * (target - current_q)
        
        # Decay epsilon more aggressively at the beginning
        if self.epsilon > self.epsilon_min:
            self.epsilon = max(self.epsilon_min, self.epsilon * self.epsilon_decay)

class LiveTrainingVisualizer:
    def __init__(self):
        plt.ion()
        self.fig = plt.figure(figsize=(16, 10))
        
        # Main game field
        self.ax_game = plt.subplot(2, 3, (1, 4))
        self.ax_game.set_xlim(0, 100)
        self.ax_game.set_ylim(0, 100)
        self.ax_game.set_aspect('equal')
        self.ax_game.set_title('Agent Live Training')
        self.ax_game.grid(True, alpha=0.3)
        
        # Score history
        self.ax_score = plt.subplot(2, 3, 2)
        self.ax_score.set_title('Score per Episode')
        self.ax_score.set_xlabel('Episode')
        self.ax_score.set_ylabel('Algae Eaten')
        self.ax_score.grid(True, alpha=0.3)
        
        # Epsilon decay
        self.ax_epsilon = plt.subplot(2, 3, 3)
        self.ax_epsilon.set_title('Exploration Rate')
        self.ax_epsilon.set_xlabel('Episode')
        self.ax_epsilon.set_ylabel('Epsilon')
        self.ax_epsilon.grid(True, alpha=0.3)
        
        # Average reward per episode
        self.ax_reward = plt.subplot(2, 3, 5)
        self.ax_reward.set_title('Average Reward per Episode')
        self.ax_reward.set_xlabel('Episode')
        self.ax_reward.set_ylabel('Avg Reward')
        self.ax_reward.grid(True, alpha=0.3)
        
        # Statistics display
        self.ax_stats = plt.subplot(2, 3, 6)
        self.ax_stats.axis('off')
        
        # Data storage
        self.scores = []
        self.epsilons = []
        self.avg_rewards = []
        self.agent_trail = deque(maxlen=15)
        
        plt.tight_layout()
        
    def update_display(self, env, agent, episode, current_reward):
        # Clear and setup game field
        self.ax_game.clear()
        self.ax_game.set_xlim(0, 100)
        self.ax_game.set_ylim(0, 100)
        self.ax_game.set_aspect('equal')
        self.ax_game.grid(True, alpha=0.3)
        
        # Agent trail
        self.agent_trail.append((env.agent_x, env.agent_y))
        if len(self.agent_trail) > 1:
            trail_x, trail_y = zip(*self.agent_trail)
            self.ax_game.plot(trail_x, trail_y, 'b-', alpha=0.6, linewidth=2, label='Agent Path')
        
        # Draw algae
        if env.algae:
            algae_x, algae_y = zip(*env.algae)
            self.ax_game.scatter(algae_x, algae_y, c='green', s=120, marker='o', 
                               label=f'Algae ({len(env.algae)})', edgecolors='darkgreen', linewidth=2)
        
        # Draw agent
        self.ax_game.scatter(env.agent_x, env.agent_y, c='red', s=200, marker='s', 
                           label='Agent', edgecolors='darkred', linewidth=2)
        
        # Line to nearest algae
        if env.algae:
            distance, nearest_idx, _, _ = env.get_nearest_algae_info()
            if nearest_idx is not None:
                nearest_x, nearest_y = env.algae[nearest_idx]
                self.ax_game.plot([env.agent_x, nearest_x], [env.agent_y, nearest_y], 
                                'r--', alpha=0.5, linewidth=1, label=f'Target (d={distance:.1f})')
        
        self.ax_game.set_title(f'Episode {episode} | Score: {env.score}/{env.num_algae} | Steps: {env.steps}')
        self.ax_game.legend(loc='upper right', fontsize=8)
        
        # Update score plot
        if len(self.scores) > 0:
            self.ax_score.clear()
            episodes_range = range(1, len(self.scores) + 1)
            self.ax_score.plot(episodes_range, self.scores, 'b-', linewidth=1, alpha=0.7)
            
            if len(self.scores) > 20:
                window = min(20, len(self.scores))
                moving_avg = [np.mean(self.scores[max(0, i-window):i+1]) for i in range(len(self.scores))]
                self.ax_score.plot(episodes_range, moving_avg, 'r-', linewidth=2, label='Moving Avg')
                self.ax_score.legend()
                
            self.ax_score.set_title('Score per Episode')
            self.ax_score.set_xlabel('Episode')
            self.ax_score.set_ylabel('Algae Eaten')
            self.ax_score.grid(True, alpha=0.3)
        
        # Update epsilon plot
        if len(self.epsilons) > 0:
            self.ax_epsilon.clear()
            self.ax_epsilon.plot(range(1, len(self.epsilons) + 1), self.epsilons, 'g-', linewidth=2)
            self.ax_epsilon.set_title('Exploration Rate')
            self.ax_epsilon.set_xlabel('Episode')
            self.ax_epsilon.set_ylabel('Epsilon')
            self.ax_epsilon.grid(True, alpha=0.3)
        
        # Update reward plot
        if len(self.avg_rewards) > 0:
            self.ax_reward.clear()
            episodes_range = range(1, len(self.avg_rewards) + 1)
            self.ax_reward.plot(episodes_range, self.avg_rewards, 'purple', linewidth=2)
            
            if len(self.avg_rewards) > 10:
                window = min(10, len(self.avg_rewards))
                moving_avg = [np.mean(self.avg_rewards[max(0, i-window):i+1]) for i in range(len(self.avg_rewards))]
                self.ax_reward.plot(episodes_range, moving_avg, 'orange', linewidth=2, label='Moving Avg')
                self.ax_reward.legend()
                
            self.ax_reward.set_title('Average Reward per Episode')
            self.ax_reward.set_xlabel('Episode')
            self.ax_reward.set_ylabel('Avg Reward')
            self.ax_reward.grid(True, alpha=0.3)
        
        # Statistics display
        self.ax_stats.clear()
        self.ax_stats.axis('off')
        
        recent_scores = self.scores[-10:] if len(self.scores) >= 10 else self.scores
        recent_avg = np.mean(recent_scores) if recent_scores else 0
        best_score = max(self.scores) if self.scores else 0
        
        stats_text = f"""TRAINING STATISTICS

Episode: {episode}
Current Score: {env.score}/{env.num_algae}
Steps: {env.steps}
Current Reward: {current_reward:.3f}
Total Episode Reward: {env.total_reward:.1f}

Agent Parameters:
Epsilon: {agent.epsilon:.4f}
Learning Rate: {agent.learning_rate}
Position: ({env.agent_x:.1f}, {env.agent_y:.1f})

Performance:
Recent Avg (10 eps): {recent_avg:.2f}
Best Score: {best_score}
Total Episodes: {len(self.scores)}
"""
        
        self.ax_stats.text(0.05, 0.95, stats_text, transform=self.ax_stats.transAxes, 
                          fontsize=10, verticalalignment='top', fontfamily='monospace')
        
        plt.tight_layout()
        plt.draw()
        plt.pause(0.001)
    
    def episode_finished(self, score, epsilon, avg_reward):
        self.scores.append(score)
        self.epsilons.append(epsilon)
        self.avg_rewards.append(avg_reward)
        self.agent_trail.clear()

def train_with_live_visualization(episodes=400, steps_per_update=3):
    env = AlgaeEnvironment()
    agent = ImprovedQLearningAgent()
    visualizer = LiveTrainingVisualizer()
    
    print("Starting RL Agent Training with Optimizations")
    print("Improvements: Better state space, reward shaping, higher learning rate")
    print("-" * 60)
    
    for episode in range(episodes):
        state = env.reset()
        step_count = 0
        
        while True:
            action = agent.choose_action(state)
            next_state, reward, done = env.step(action)
            agent.learn(state, action, reward, next_state, done)
            
            state = next_state
            step_count += 1
            
            # Update visualization
            if step_count % steps_per_update == 0 or reward > 50 or done:
                visualizer.update_display(env, agent, episode + 1, reward)
            
            if done:
                break
        
        # Episode finished
        avg_reward = env.total_reward / env.steps if env.steps > 0 else 0
        visualizer.episode_finished(env.score, agent.epsilon, avg_reward)
        visualizer.update_display(env, agent, episode + 1, 0)
        
        # Console output every 25 episodes
        if (episode + 1) % 25 == 0:
            recent_scores = visualizer.scores[-25:]
            avg_score = np.mean(recent_scores)
            avg_reward_recent = np.mean(visualizer.avg_rewards[-25:]) if len(visualizer.avg_rewards) >= 25 else 0
            print(f"Episode {episode + 1:3d} | Avg Score: {avg_score:5.2f} | Avg Reward: {avg_reward_recent:6.1f} | Epsilon: {agent.epsilon:.4f}")
    
    print("\nTraining completed!")
    print(f"Final performance: {np.mean(visualizer.scores[-50:]):.2f} avg score (last 50 episodes)")
    
    return agent, visualizer.scores

def demo_trained_agent(agent, episodes=3):
    env = AlgaeEnvironment()
    
    print(f"\nDemonstrating trained agent ({episodes} episodes)")
    print("Agent uses learned policy only (epsilon=0)")
    
    fig, ax = plt.subplots(figsize=(10, 8))
    
    for episode in range(episodes):
        state = env.reset()
        old_epsilon = agent.epsilon
        agent.epsilon = 0  # No random actions
        
        path_x = [env.agent_x]
        path_y = [env.agent_y]
        
        step_count = 0
        while True:
            action = agent.choose_action(state)
            next_state, reward, done = env.step(action)
            
            path_x.append(env.agent_x)
            path_y.append(env.agent_y)
            state = next_state
            step_count += 1
            
            if done or step_count > 150:
                break
        
        # Visualize episode
        ax.clear()
        ax.set_xlim(0, 100)
        ax.set_ylim(0, 100)
        ax.set_aspect('equal')
        
        # Remaining algae
        if env.algae:
            algae_x, algae_y = zip(*env.algae)
            ax.scatter(algae_x, algae_y, c='green', s=100, label='Remaining Algae')
        
        # Agent path
        ax.plot(path_x, path_y, 'b-', linewidth=2, alpha=0.7, label='Agent Path')
        ax.scatter(path_x[0], path_y[0], c='blue', s=150, marker='s', label='Start')
        ax.scatter(path_x[-1], path_y[-1], c='red', s=150, marker='X', label='End')
        
        ax.set_title(f'Demo Episode {episode + 1} | Score: {env.score}/{env.num_algae} | Steps: {step_count}')
        ax.grid(True, alpha=0.3)
        ax.legend()
        
        plt.draw()
        plt.pause(1.5)
        
        print(f"  Episode {episode + 1}: {env.score}/{env.num_algae} algae in {step_count} steps")
        
        agent.epsilon = old_epsilon
    
    plt.show()

if __name__ == "__main__":
    print("REINFORCEMENT LEARNING ALGAE COLLECTOR")
    print("=" * 50)
    
    # Start live training with optimizations
    trained_agent, training_scores = train_with_live_visualization(episodes=1000, steps_per_update=2)
    
    # Demo after training
    input("\nPress Enter to see trained agent demo...")
    demo_trained_agent(trained_agent, episodes=3)
    
    input("\nTraining and demo completed! Press Enter to exit...")