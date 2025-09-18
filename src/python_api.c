// python_api_v2.c - RL Python interface with 4 inputs
#include <Python.h>
#include <stdio.h>

#include "types.h"
#include "python_api.h"
#include "fish.h"
#include "simulation.h"
#include "nutrition.h"

static PyObject* g_python_module = NULL;
static PyObject* g_update_function = NULL;

// Python C API functions for RL system

static PyObject* py_fish_add(PyObject* self, PyObject* args) {
    (void)self;
    float x, y;
    int fish_type;
    
    if (!PyArg_ParseTuple(args, "ffi", &x, &y, &fish_type)) {
        return NULL;
    }
    
    int fish_id = fish_add(x, y, fish_type);
    return PyLong_FromLong(fish_id);
}

static PyObject* py_fish_get_count(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    return PyLong_FromLong(fish_get_count());
}

static PyObject* py_fish_get_position(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    Node* nodes = simulation_get_nodes();
    if (fish->node_id < 0 || fish->node_id >= simulation_get_node_count()) {
        Py_RETURN_NONE;
    }
    
    Node* node = &nodes[fish->node_id];
    return Py_BuildValue("(ff)", node->x, node->y);
}

static PyObject* py_fish_get_heading(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        return PyFloat_FromDouble(0.0);
    }
    
    return PyFloat_FromDouble(fish->heading);
}

// UPDATED: Get RL inputs (now 4 inputs)
static PyObject* py_fish_get_rl_inputs(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    return Py_BuildValue("(ffff)", 
                         fish->rl_inputs[0],  // plant_vector_x
                         fish->rl_inputs[1],  // plant_vector_y
                         fish->rl_inputs[2],  // oxygen_level
                         fish->rl_inputs[3]); // plant_distance (normalized)
}

// Set RL outputs
static PyObject* py_fish_set_rl_outputs(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    float turn_direction, movement_strength, eat_command;
    
    if (!PyArg_ParseTuple(args, "ifff", &fish_id, &turn_direction, &movement_strength, &eat_command)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    fish->rl_outputs[0] = turn_direction;   // -1.0 to 1.0
    fish->rl_outputs[1] = movement_strength; // 0.0 to 1.0
    fish->rl_outputs[2] = eat_command;       // 0.0 to 1.0
    
    // DEBUG: Track RL output setting for multiple fish
    static int debug_counters[MAX_FISH] = {0};
    
    if (fish_id < MAX_FISH && (debug_counters[fish_id]++ % 90) == 0) {  // Every 3 seconds
        printf("DEBUG py_fish_set_rl_outputs: Fish %d outputs set to (%.3f, %.3f, %.3f)\n",
               fish_id, turn_direction, movement_strength, eat_command);
    }
    
    Py_RETURN_NONE;
}

static PyObject* py_fish_get_last_reward(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    float reward = fish_get_last_reward(fish_id);
    return PyFloat_FromDouble(reward);
}

static PyObject* py_fish_get_stomach_contents(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        return PyFloat_FromDouble(0.0);
    }
    
    return PyFloat_FromDouble(fish->stomach_contents);
}

static PyObject* py_fish_is_eating(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        return PyLong_FromLong(0);
    }
    
    return PyLong_FromLong(fish->eating_mode);
}

static PyObject* py_fish_get_type_count(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    return PyLong_FromLong(fish_get_type_count());
}

static PyObject* py_get_world_bounds(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    return Py_BuildValue("(ffff)", WORLD_LEFT, WORLD_TOP, WORLD_RIGHT, WORLD_BOTTOM);
}

static PyObject* py_get_nutrition_balance(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    float fish_consumed = fish_get_total_nutrition_consumed();
    float fish_defecated = fish_get_total_nutrition_defecated();
    float env_added = nutrition_get_total_added();
    float env_depleted = nutrition_get_total_depleted();
    
    return Py_BuildValue("(ffff)", fish_consumed, fish_defecated, env_added, env_depleted);
}

static PyObject* py_get_rl_info(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    return Py_BuildValue("(ii)", RL_INPUT_SIZE, RL_OUTPUT_SIZE);
}

// Legacy compatibility functions (return dummy values)
static PyObject* py_fish_get_energy(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        return PyFloat_FromDouble(1.0);
    }
    
    return PyFloat_FromDouble(fish->energy);
}

static PyObject* py_fish_get_velocity(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    Node* nodes = simulation_get_nodes();
    if (fish->node_id < 0 || fish->node_id >= simulation_get_node_count()) {
        Py_RETURN_NONE;
    }
    
    Node* node = &nodes[fish->node_id];
    return Py_BuildValue("(ff)", node->vx, node->vy);
}

static PyObject* py_fish_get_consumed_nutrition(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        return PyFloat_FromDouble(0.0);
    }
    
    return PyFloat_FromDouble(fish->consumed_nutrition);
}

// Legacy functions that no longer apply but kept for compatibility
static PyObject* py_fish_eat_nearby_plants(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    // Eating is now handled by RL outputs
    Py_RETURN_NONE;
}

static PyObject* py_fish_set_movement_force(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    // Movement is now handled by RL outputs
    Py_RETURN_NONE;
}

static PyObject* py_fish_apply_rl_action(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    // Replaced by py_fish_set_rl_outputs
    Py_RETURN_NONE;
}

static PyObject* py_fish_get_vision_ray(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    // No longer used in RL system
    return PyFloat_FromDouble(1.0);
}

static PyObject* py_fish_get_nutrition_ray(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    // No longer used in RL system
    return PyFloat_FromDouble(0.0);
}

static PyObject* py_fish_get_oxygen_level(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    // Return current environmental oxygen
    return PyFloat_FromDouble(fish_get_oxygen_level(fish_id));
}

static PyObject* py_fish_get_hunger_level(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    return PyFloat_FromDouble(fish_get_hunger_level(fish_id));
}

static PyObject* py_fish_get_saturation_level(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    return PyFloat_FromDouble(fish_get_saturation_level(fish_id));
}

static PyObject* py_get_vision_info(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    // Legacy compatibility - return dummy values
    return Py_BuildValue("(ii)", 12, 12);
}

// Method definitions for the Python module (UPDATED)
static PyMethodDef SimulationMethods[] = {
    {"fish_add", py_fish_add, METH_VARARGS, "Add a fish to the simulation"},
    {"fish_get_count", py_fish_get_count, METH_NOARGS, "Get total fish count"},
    {"fish_get_position", py_fish_get_position, METH_VARARGS, "Get fish position"},
    {"fish_get_heading", py_fish_get_heading, METH_VARARGS, "Get fish heading in radians"},
    {"fish_get_rl_inputs", py_fish_get_rl_inputs, METH_VARARGS, "Get RL inputs (plant_vec_x, plant_vec_y, oxygen, distance)"},
    {"fish_set_rl_outputs", py_fish_set_rl_outputs, METH_VARARGS, "Set RL outputs (turn, movement, eat)"},
    {"fish_get_last_reward", py_fish_get_last_reward, METH_VARARGS, "Get fish last reward"},
    {"fish_get_stomach_contents", py_fish_get_stomach_contents, METH_VARARGS, "Get fish stomach contents"},
    {"fish_is_eating", py_fish_is_eating, METH_VARARGS, "Check if fish is in eating mode"},
    {"fish_get_type_count", py_fish_get_type_count, METH_NOARGS, "Get fish type count"},
    {"get_world_bounds", py_get_world_bounds, METH_NOARGS, "Get world boundaries"},
    {"get_nutrition_balance", py_get_nutrition_balance, METH_NOARGS, "Get nutrition cycle balance"},
    {"get_rl_info", py_get_rl_info, METH_NOARGS, "Get RL system info (input_size, output_size)"},
    
    // Legacy compatibility functions
    {"fish_get_energy", py_fish_get_energy, METH_VARARGS, "Get fish energy (legacy)"},
    {"fish_get_velocity", py_fish_get_velocity, METH_VARARGS, "Get fish velocity (legacy)"},
    {"fish_get_consumed_nutrition", py_fish_get_consumed_nutrition, METH_VARARGS, "Get fish consumed nutrition"},
    {"fish_eat_nearby_plants", py_fish_eat_nearby_plants, METH_VARARGS, "Legacy eating function"},
    {"fish_set_movement_force", py_fish_set_movement_force, METH_VARARGS, "Legacy movement function"},
    {"fish_apply_rl_action", py_fish_apply_rl_action, METH_VARARGS, "Legacy RL action function"},
    {"fish_get_vision_ray", py_fish_get_vision_ray, METH_VARARGS, "Legacy vision ray function"},
    {"fish_get_nutrition_ray", py_fish_get_nutrition_ray, METH_VARARGS, "Legacy nutrition ray function"},
    {"fish_get_oxygen_level", py_fish_get_oxygen_level, METH_VARARGS, "Get environmental oxygen level"},
    {"fish_get_hunger_level", py_fish_get_hunger_level, METH_VARARGS, "Get fish hunger level"},
    {"fish_get_saturation_level", py_fish_get_saturation_level, METH_VARARGS, "Get fish saturation level"},
    {"get_vision_info", py_get_vision_info, METH_NOARGS, "Legacy vision info function"},
    
    {NULL, NULL, 0, NULL}
};

// Module definition
static struct PyModuleDef simulation_module = {
    PyModuleDef_HEAD_INIT,
    "simulation",
    "Marine ecosystem simulation API with RL fish control v2 (4 inputs)",
    -1,
    SimulationMethods,
    NULL,
    NULL,
    NULL,
    NULL
};

// Module initialization function
PyMODINIT_FUNC PyInit_simulation(void) {
    return PyModule_Create(&simulation_module);
}

int python_api_init(void) {
    if (PyImport_AppendInittab("simulation", PyInit_simulation) == -1) {
        printf("Failed to add simulation module to Python\n");
        return 0;
    }
    
    Py_Initialize();
    if (!Py_IsInitialized()) {
        printf("Failed to initialize Python interpreter\n");
        return 0;
    }
    
    printf("Python API v2 initialized for RL fish control (4 inputs)\n");
    return 1;
}

void python_api_cleanup(void) {
    if (g_update_function) {
        Py_DECREF(g_update_function);
        g_update_function = NULL;
    }
    
    if (g_python_module) {
        Py_DECREF(g_python_module);
        g_python_module = NULL;
    }
    
    if (Py_IsInitialized()) {
        Py_Finalize();
    }
}

int python_api_run_script(const char* script_path) {
    FILE* fp = fopen(script_path, "r");
    if (!fp) {
        printf("Failed to open Python script: %s\n", script_path);
        return 0;
    }
    
    if (PyRun_SimpleFile(fp, script_path) != 0) {
        printf("Failed to execute Python script: %s\n", script_path);
        fclose(fp);
        return 0;
    }
    
    fclose(fp);
    
    g_python_module = PyImport_ImportModule("__main__");
    if (g_python_module) {
        g_update_function = PyObject_GetAttrString(g_python_module, "update_fish");
        if (!g_update_function || !PyCallable_Check(g_update_function)) {
            if (g_update_function) {
                Py_DECREF(g_update_function);
                g_update_function = NULL;
            }
            printf("Warning: No callable 'update_fish' function found in Python script\n");
        } else {
            printf("Python script v2 loaded successfully with RL support (4 inputs)\n");
        }
    }
    
    return 1;
}

void python_api_update(void) {
    if (!g_update_function) return;
    
    PyObject* result = PyObject_CallObject(g_update_function, NULL);
    
    if (result) {
        Py_DECREF(result);
    } else {
        if (PyErr_Occurred()) {
            PyErr_Print();
        }
    }
}