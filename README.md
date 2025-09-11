# ThinkingBlue

**Marine Ecosystem Simulation with Reinforcement Learning Agents**

A complex marine ecosystem simulation featuring autonomous fish agents that learn to survive using reinforcement learning. The simulation models a Great Barrier Reef environment with dynamic plant growth, oxygen production, nutrition cycles, and intelligent fish behavior.

<img width="1150" height="760" alt="image" src="https://github.com/user-attachments/assets/31635e8e-5fae-415b-a139-6eb006f53fa5" />

## Features

### **Ecosystem Simulation**
- **8 Marine Plant Species**: Including Kelp, Sea Grass, Corals, and Sponges with distinct growth patterns
- **6 Fish Species**: Each with unique behavioral characteristics and metabolic rates  
- **Dynamic Plant Growth**: Plants grow, spread, and compete for resources based on environmental conditions
- **Oxygen Production**: Plants produce oxygen that affects fish survival and behavior
- **Nutrition Cycling**: Complete nutrient cycle with plant consumption, fish digestion, and waste production

### **Reinforcement Learning Fish Agents**
- **Enhanced Vision System**: 180° field of view with 12 vision rays for obstacle detection
- **Chemoreceptor System**: 12 additional sensing rays that detect nutrition gradients from plants
- **Intelligent Behavior**: Fish learn to find food, avoid obstacles, and manage oxygen levels
- **Neural Network**: Deep 4-layer network (27→32→16→8→2) for complex decision making
- **Reward System**: Sophisticated reward structure based on nutrition quality, oxygen levels, and survival

### **Advanced Systems**
- **Spatial Optimization**: Grid-based collision detection and spatial partitioning
- **Physics Engine**: Realistic water physics with drag, repulsion forces, and movement dynamics
- **Environmental Layers**: Oxygen and nutrition heatmaps with visual rendering
- **Real-time Rendering**: SDL2-based graphics with camera controls and layer visualization

### **Technical Architecture**
- **C Backend**: Core simulation engine written in C for performance
- **Python RL Integration**: Reinforcement learning agents implemented in Python with NumPy
- **Modular Design**: Separate systems for physics, rendering, plant growth, and fish behavior
- **Configuration-Driven**: Plant and fish properties defined in external config files

## System Requirements

- **MSYS2/MinGW64** development environment
- **SDL2** graphics library
- **Python 3.x** with development headers
- **NumPy** for neural network computations

## Building

```bash
# Install dependencies (MSYS2)
make install-deps

# Build the project
make rebuild

# Run the simulation
make run
```

## Controls

- **WASD**: Camera movement
- **Shift+WASD**: Sprint camera movement  
- **Mouse Wheel**: Zoom in/out
- **Left Click**: Place organisms
- **Right Click**: Connect plants (chain creation)
- **Tab**: Toggle between plant and fish placement modes
- **1-8**: Select plant species
- **F1-F6**: Select fish species
- **N**: Toggle nutrition layer visualization
- **G**: Toggle oxygen layer visualization  
- **R**: Toggle fish vision ray rendering
- **P**: Print debug information

## Configuration

### **Plant Species** (`plants.conf`)
Configure 8 marine plant species with properties like:
- Growth rates and patterns
- Nutrition values for fish consumption
- Oxygen production capabilities
- Visual appearance and aging effects

### **Fish Species** (`fish.conf`)  
Configure 6 fish species with properties like:
- Swimming speed and agility
- Vision and chemoreceptor ranges
- Metabolic rates and feeding behavior
- Oxygen consumption and tolerance

## Reinforcement Learning

Fish agents use deep reinforcement learning to learn optimal survival strategies:

- **State Space**: 27-dimensional input including vision, chemical detection, and internal status
- **Action Space**: Continuous turn and speed actions
- **Learning Algorithm**: Custom gradient-based learning with experience replay
- **Exploration**: Adaptive epsilon-greedy with biologically-motivated emergency behaviors
- **Reward Structure**: Multi-component rewards for nutrition quality, oxygen management, and survival

## Scientific Accuracy

The simulation incorporates realistic marine biology concepts:

- **Chemoreception**: Fish detect chemical gradients to locate food sources
- **Photosynthesis**: Plants produce oxygen affecting local environment  
- **Nutrient Cycling**: Waste products fertilize plant growth
- **Territorial Behavior**: Spatial competition and resource distribution
- **Metabolic Processes**: Oxygen consumption, digestion, and energy management

## Oxygen-Simulation
<img width="994" height="773" alt="image" src="https://github.com/user-attachments/assets/5a882de7-89ba-4ebe-aebe-527cec990c0e" />g

## Reinforcement Learning Fish Agents
<img width="1189" height="785" alt="image" src="https://github.com/user-attachments/assets/94385059-1019-40da-80e0-df2007b99bd2" />

## Nutrient Cycle
<img width="1183" height="789" alt="image" src="https://github.com/user-attachments/assets/8246baaf-698a-488f-97eb-7b670d5c6fa7" />
