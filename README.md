<img width="3840" height="659" alt="ThinkBlue" src="https://github.com/user-attachments/assets/cdb42ab5-0391-411b-b910-5490537372b6" />

**Marine Ecosystem Simulation with Reinforcement Learning Agents**

Transforming the complexity of marine ecosystems into a living laboratory - where environmental scenarios play out in minutes, revealing adaptation patterns and informing strategies to protect our oceans.

<img width="1431" height="577" alt="thinkbluebanner2" src="https://github.com/user-attachments/assets/99784377-ee13-45ee-b830-726641687e70" />

## Core Features

### **Dynamic Ecosystem Simulation**
- **20 Marine Plant Species**: Hard corals, soft corals, marine plants, and other organisms with realistic growth patterns
- **13 Fish Species**: From small herbivores to apex predators, each with unique behavioral characteristics
- **Plant Growth Dynamics**: Plants grow, spread, compete for resources, and respond to environmental conditions
- **Oxygen Production**: Plants generate oxygen that creates environmental gradients affecting fish survival
- **Nutrient Cycling**: Complete ecosystem with plant consumption, fish digestion, waste production, and decomposition

### **Reinforcement Learning Fish Agents**
- **Advanced Vision System**: 180° field of view vector that points to nearest plants & predators
- **Neural Network Architecture**: 3-layer deep network (7→20→3) with Xavier initialization
- **Intelligent Behavior**: Fish learn optimal foraging, predator avoidance, and oxygen management strategies
- **Sophisticated Reward System**: Multi-component rewards based on nutrition quality, oxygen levels, movement efficiency, and survival

### **Advanced Simulation Systems**
- **Spatial Optimization**: Grid-based collision detection and spatial partitioning for performance
- **Physics Engine**: Realistic underwater physics with drag forces, repulsion dynamics, and flow fields
- **Environmental Layers**: Real-time oxygen and nutrition heatmaps with visual rendering
- **Temperature Control**: Coral bleaching simulation with temperature-dependent ecosystem responses
- **Statistics Monitoring**: Real-time ecosystem metrics with GUI controls for environmental parameters

## Technical Architecture

### **Hybrid C/Python Implementation**
- **C Backend**: High-performance simulation core handling physics, rendering, and ecosystem dynamics
- **Python Integration**: Reinforcement learning agents implemented in Python with C API integration
- **Modular Design**: Separate systems for physics, rendering, plant growth, fish behavior, and environmental layers
- **Configuration-Driven**: Organism properties defined in external configuration files for easy modification

### **Neural Network System**
The fish use a sophisticated neural network architecture:

**Input Layer (7 neurons):**
- `target_vector_x`: X-component of direction to nearest food source
- `target_vector_y`: Y-component of direction to nearest food source  
- `oxygen_level`: Current oxygen level at fish position (0.0-1.0)
- `target_distance`: Distance to nearest food source
- `threat_vector_x`: X-component of direction to nearest predator
- `threat_vector_y`: Y-component of direction to nearest predator
- `danger_level`: Threat assessment of nearby predators

**Hidden Layer (20 neurons):**
- ReLU activation with Xavier/Glorot weight initialization
- Biased toward exploration in early learning phases

**Output Layer (3 neurons):**
- `turn_direction`: Steering control (-1.0 to 1.0, left to right)
- `movement_strength`: Speed control (0.0 to 1.0, stop to full speed)
- `eat_command`: Feeding behavior (0.0 to 1.0, ignore to consume)

**Learning Algorithm:**
- Gradient-based learning with experience replay
- Adaptive epsilon-greedy exploration (starts at 0.6, decays to 0.05)
- Momentum-based action smoothing to prevent erratic movement
- Parent-child neural network inheritance during reproduction

### **Environmental Systems**

**Plant Growth Simulation:**
- Individual species have distinct growth probabilities and branching patterns
- Plants compete for space and resources through nutrition depletion
- Oxygen production varies by species and environmental conditions
- Aging effects change plant appearance and productivity over time

**Predator-Prey Dynamics:**
- Herbivorous fish target plants using chemoreceptor sensing
- Predatory fish hunt other fish using visual detection systems
- Eating cooldowns prevent unrealistic feeding behavior
- Corpse system provides nutrition sources after fish death

**Flow Dynamics:**
- Dynamic water current simulation affects fish movement and behavior
- Flow fields create realistic underwater movement patterns with directional forces
- Fish learn to use currents for efficient transportation and energy conservation
- Flow strength varies spatially creating micro-environments within the ecosystem
- Current patterns influence plant growth and particle distribution

**Oxygen and Nutrition Layers:**
- Plants produce oxygen in radius-based gradients
- Fish consume oxygen based on activity and species metabolism  
- Nutrition gradients guide fish toward food sources
- Waste products from fish digestion fertilize plant growth

## Installation and Setup

### **System Requirements**
- **MSYS2/MinGW64** development environment (Windows)
- **SDL2** graphics library for rendering
- **Python 3.12** with development headers for neural networks
- **GCC compiler** with C99 standard support

### **Environment Setup**

The Python environment must be configured correctly for the neural network system to function:

```bash
# Set environment variables (REQUIRED)
export PATH="/mingw64/bin:$PATH"
export PYTHONHOME=/mingw64 
export PYTHONPATH=/mingw64/lib/python3.12
```

### **Installation Steps**

1. **Install MSYS2/MinGW64** from https://www.msys2.org/

2. **Install dependencies:**
```bash
# Install required packages
make install-deps

# Or manually:
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-SDL2 \
    mingw-w64-x86_64-python \
    mingw-w64-x86_64-python-pip
```

3. **Set environment variables:**
```bash
export PATH="/mingw64/bin:$PATH"
export PYTHONHOME=/mingw64 
export PYTHONPATH=/mingw64/lib/python3.12
```

4. **Build the project:**
```bash
# Clean build
make rebuild

# Check Python integration
make check-python
```

5. **Run the simulation:**
```bash
make run
```

### **Troubleshooting**

**Python Integration Issues:**
- Ensure environment variables are set correctly
- Verify Python headers: `test -f /mingw64/include/python3.12/Python.h`
- Check library paths: `ls /mingw64/lib/libpython3.12*`

**Build Errors:**
- Use `make build-no-check` to skip Python verification
- Check dependencies with `make install-deps`
- Verify SDL2 installation: `pkg-config --cflags --libs sdl2`

**Runtime Problems:**
- Check file permissions on configuration files
- Ensure `fish_controller.py` is in the project root
- Monitor console output for specific error messages

## Configuration System

### **Plant Species Configuration** (`plants.conf`)

Configure 20 marine plant species with detailed properties:

```ini
[StaghornCoral]
growth_probability=0.003          # Probability of growth each frame
growth_attempts=5                 # Growth attempts per frame
max_branches=8                    # Maximum branching complexity
branch_distance=55.0              # Distance between growth nodes
mobility_factor=0.05              # Movement flexibility
age_mature=2400                   # Frames to reach maturity
nutrition_depletion_strength=0.08 # Resource consumption impact
oxygen_production_factor=0.3      # Oxygen generation rate
oxygen_production_radius=90.0     # Oxygen effect radius
node_size_factor=1.0              # Visual size scaling
chain_thickness_factor=1.0        # Connection thickness
chain_curvature_factor=1.2        # Growth curvature
node_color=FF6B35                 # Plant node color (hex)
chain_color=E55100                # Connection color (hex)
```

**Available Plant Species:**
- **Hard Corals (9 species)**: StaghornCoral, TableCoral, BrainCoral, ElkhornCoral, PlantCoral, CoralletCoral, BushyCoral, CoralTree
- **Soft Corals and Gorgonians (5 species)**: SoftCoral, SeaFan, SeaPlume, SeaWhip, GoldenSeaRod
- **Marine Plants (3 species)**: Kelp, GiantKelp, SeaGrass
- **Specialized Organisms (3 species)**: Sponge, AnemoneGarden, BlackCoral

### **Fish Species Configuration** (`fish.conf`)

Configure 13 fish species with behavioral and neural network parameters:

```ini
[ParrotFish]
max_speed=8.0                     # Maximum swimming speed
max_force=2.0                     # Turning/acceleration force
mass=1.2                          # Physical mass for physics
size_radius=7.0                   # Collision detection radius
eating_range=60.0                 # Distance to consume food
fov_angle=220.0                   # Field of view (degrees)
max_turn_angle=55.0               # Maximum turning per frame
oxygen_reward_factor=0.015        # Reward scaling for oxygen
proximity_reward_factor=0.01      # Reward for approaching food
eat_punishment=-0.02              # Penalty for failed eating attempts
flow_sensitivity=0.25             # Response to water currents
danger_level=0.1                  # Threat level to other fish
is_predator=0                     # 0=herbivore, 1=predator
eating_cooldown_frames=0          # Frames between eating attempts
fish_detection_range=280.0        # Range to detect other fish
max_age=36000                     # Maximum lifespan (frames)
node_size_factor=0.8              # Visual size scaling
tail_length_factor=1.0            # Tail length scaling
tail_width_factor=1.0             # Tail width scaling
node_color=1E90FF                 # Fish color (hex)
```

**Available Fish Species:**
- **Herbivorous Reef Fish (6 species)**: ClownFish, AngelFish, Parrotfish, SurgeonTang, Butterflyfish, Wrasse
- **Predatory Fish (7 species)**: CoralTrout, Barracuda, ReefShark, TigerShark, GiantMorayEel, Grouper

## Controls and User Interface

### **Camera and Navigation**
- **WASD**: Move camera around the ecosystem
- **Shift+WASD**: Sprint camera movement for faster navigation
- **Mouse Wheel**: Zoom in/out (unlimited zoom range)
- **Mouse Drag**: Alternative camera movement

### **Organism Placement**
- **Left Click**: Place selected organism at cursor position
- **Right Click**: Create plant chains (plant mode only)
- **Tab**: Open comprehensive statistics monitor with temperature control
- **Shift+Tab**: Toggle between plant and fish placement modes

### **Species Selection**
- **1-8**: Select plant species
- **F1-F6**: Select fish species

### **Visualization Layers**
- **N**: Toggle nutrition layer visualization (shows food gradients)
- **G**: Toggle oxygen layer visualization (shows gas distribution)
- **F**: Toggle flow field visualization (shows water currents)
- **R**: Toggle fish vision ray rendering (shows AI perception)

### **System Controls**
- **P**: Print detailed debug information to console
- **ESC**: Exit simulation gracefully

### **Statistics Monitor**

Press **Tab** to open the real-time ecosystem statistics window featuring:

- **Population Tracking**: Live counts of fish and plant populations
- **Temperature Control**: Slider to adjust ecosystem temperature (0.0°C to 3.0°C)
- **Coral Bleaching**: Visual monitoring of temperature effects on coral health
- **Neural Network Performance**: Tracking of AI learning progress and reproduction success
- **Environmental Metrics**: nutrition balance and ecosystem health

## Neural Network Learning System

### **Training Process**

Fish agents learn through continuous interaction with the environment:

1. **Sensory Input**: Each fish processes 7 environmental inputs every frame
2. **Decision Making**: Neural network outputs 3 action values
3. **Action Execution**: Fish performs movement and feeding behaviors
4. **Reward Calculation**: Multi-component reward based on survival factors
5. **Learning Update**: Weights adjusted using gradient descent with experience replay

### **Reward Structure**

**Positive Rewards:**
* **Proximity to food sources**: Strong rewards for finding plants (herbivores) or prey fish (predators)
* **Successful consumption**: Large rewards for actual eating of plants, fish, or corpses
* **Predator avoidance**: Rewards for escaping from threats (herbivores fleeing from predators)
* **Reproduction success**: Large rewards (+150-200) for successful reproduction events
* **Basic survival**: Small continuous reward (+0.001) just for staying alive
* **Oxygen level rewards**: Rewards based on current oxygen level (species-specific oxygen_reward_factor)

**Negative Rewards:**
* **Failed eating attempts**: Punishment values from configuration (eat_punishment: -0.015 to -0.05)
* **Excessive spinning**: Penalties for high turn amounts (>0.6) and sustained spinning behavior
* **Approaching threats**: Penalties for moving toward predators (herbivores) or stronger predators
* **Stillness when prey visible**: Predators get penalized for not pursuing visible prey

### **Evolution and Inheritance**

When fish reproduce:
- Parent neural networks are copied to offspring with small random mutations
- Successful parents (high reproduction count + reward) are more likely to pass on traits
- Generation tracking allows monitoring of evolutionary progress
- Best-performing models are automatically saved for analysis

### **Model Persistence**

The system tracks neural network performance and evolution:
- **Performance Tracking**: Monitors reproduction success, survival time, and reward accumulation
- **Generation Tracking**: Allows monitoring of evolutionary progress across fish generations
- **Best Performer Identification**: Automatically identifies and tracks the most successful neural networks

## Scientific Accuracy and Biological Modeling

### **Marine Biology Concepts**

**Direct Target Detection:**
- Fish use line-of-sight algorithms to locate food sources within their field of view
- Detection range and field of view angle vary by species (110°-260° FOV)
- Fish efficiently identify the nearest available food source or threat

**Photosynthesis and Oxygen Production:**
- Plants generate oxygen at species-specific rates (oxygen_production_factor: 0.05-0.85)
- Oxygen diffuses through water creating environmental gradients
- Fish consume oxygen based on activity level and species metabolism

**Nutrient Cycling:**
- Plant consumption depletes local nutrition sources
- Fish digestion produces waste that fertilizes nearby plants
- Corpses from dead fish provide additional nutrition sources
- Ecosystem maintains dynamic equilibrium through these cycles

**Territorial and Social Behavior:**
- Spatial competition for resources drives territorial behavior
- Predator-prey relationships create realistic hunting dynamics
- Field of view limitations create natural territorial boundaries
- Individual fish behaviors aggregate into emergent group patterns

### **Environmental Factors**

**Temperature Effects:**
- Coral bleaching occurs when temperature exceeds optimal ranges
- Bleached corals appear white/gray and stop producing oxygen
- Temperature affects entire coral colonies including growth connections
- Higher temperatures increase bleaching probability exponentially

**Flow Dynamics:**
- Water currents influence fish movement and energy expenditure
- Flow fields create realistic underwater movement patterns
- Fish learn to use currents for efficient transportation

## Development and Extensibility

### **Adding New Species**

**New Plant Species:**
1. Add configuration section to `plants.conf`
2. Specify growth parameters, visual properties, and environmental effects
3. Plants are automatically loaded and available for placement

**New Fish Species:**
1. Add configuration section to `fish.conf`
2. Define behavioral parameters, neural network settings, and visual properties
3. Fish inherit the same neural network architecture with species-specific tuning

### **Extending Neural Networks**

**Modifying Network Architecture:**
1. Adjust `RL_INPUT_SIZE` and `RL_OUTPUT_SIZE` in `types.h`
2. Update input generation in `fish_update_rl_inputs()`
3. Modify output handling in `fish_apply_rl_outputs()`
4. Update Python neural network class in `fish_controller.py`

**Adding New Sensors:**
1. Implement sensor logic in fish behavior system
2. Add input processing to neural network
3. Update reward calculation for new sensory information

### **Building Custom Scenarios**

**Environmental Presets:**
- Modify `populate_reef_randomly()` in `main.c` for custom initial conditions
- Adjust temperature, plant distribution, and fish populations
- Create specialized ecosystems for research or demonstration

**Experimental Parameters:**
- Tune learning rates and exploration parameters in `fish_controller.py`
- Modify reward structures for different behavioral goals
- Implement new fitness metrics for evolutionary analysis

## Research Applications

### **Educational Use Cases**

**Marine Biology Education:**
- Visualize complex ecosystem interactions
- Demonstrate predator-prey dynamics
- Explore effects of environmental changes

**Artificial Intelligence Learning:**
- Study emergent behaviors in multi-agent systems
- Analyze neural network learning in dynamic environments
- Compare different reinforcement learning approaches

**Environmental Science:**
- Model climate change effects on coral reefs
- Simulate coral bleaching scenarios
- Study ecosystem resilience and recovery

### **Research Extensions**

**Possible Research Directions:**
- Genetic algorithms for neural network evolution
- Multi-species cooperation and competition dynamics
- Environmental disturbance response patterns
- Collective intelligence and swarm behaviors
- Climate change impact modeling

**Data Collection:**
- Neural network performance metrics
- Population dynamics over time
- Behavioral pattern analysis
- Environmental response studies

## Troubleshooting and FAQ

### **Common Issues**

**Q: Simulation fails to start with Python errors**
A: Ensure the required environment variables are set correctly.

**Q: Fish are not moving or learning**
A: Check that `fish_controller.py` loaded successfully. Look for "Neural network script loaded successfully!" in console output.

**Q: Build fails with SDL2 errors**
A: Install SDL2 development libraries: `pacman -S mingw-w64-x86_64-SDL2`

**Q: Performance is slow with many entities**
A: Reduce initial population counts or disable expensive visual layers (nutrition, oxygen rendering).

**Q: Neural networks are not learning effectively**
A: Adjust learning parameters in `fish_controller.py`. Increase `learning_rate` or modify `exploration_rate` decay.

**Q: Statistics window doesn't open**
A: Ensure Python tkinter is available: `python -c "import tkinter"`
