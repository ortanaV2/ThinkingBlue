#include <Python.h>
#include <stdio.h>

#include "types.h"
#include "python_api.h"
#include "fish.h"
#include "simulation.h"
#include "nutrition.h"
#include "temperature.h"
#include "plants.h"

static PyObject* g_python_module = NULL;
static PyObject* g_update_function = NULL;

// Temperature control functions
static PyObject* py_temperature_get_current(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    
    float temp = temperature_get_current();
    return PyFloat_FromDouble(temp);
}

static PyObject* py_temperature_set_current(PyObject* self, PyObject* args) {
    (void)self;
    float temp;
    
    if (!PyArg_ParseTuple(args, "f", &temp)) {
        return NULL;
    }
    
    temperature_set_current(temp);
    Py_RETURN_NONE;
}

static PyObject* py_temperature_get_bleached_count(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    int bleached_count = 0;
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active && temperature_is_coral_bleached(i)) {
            bleached_count++;
        }
    }
    
    return PyLong_FromLong(bleached_count);
}

// Plant population statistics
static PyObject* py_get_plant_node_count(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    
    Node* nodes = simulation_get_nodes();
    int node_count = simulation_get_node_count();
    int plant_count = 0;
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].active && nodes[i].plant_type >= 0) {
            plant_count++;
        }
    }
    
    return PyLong_FromLong(plant_count);
}

static PyObject* py_get_total_environmental_nutrition(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    
    float total_nutrition = plants_get_total_environmental_nutrition();
    return PyFloat_FromDouble(total_nutrition);
}

// Fish aging system
static PyObject* py_fish_get_age_info(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) {
        Py_RETURN_NONE;
    }
    
    int current_frame = simulation_get_frame_counter();
    int age = current_frame - fish->birth_frame;
    float age_ratio = (float)age / (float)fish_type->max_age;
    
    return Py_BuildValue("(iifi)", 
                         age,                    // Current age in frames
                         fish_type->max_age,     // Max age in frames
                         age_ratio,              // Age ratio (0.0 to 1.0+)
                         fish->birth_frame);     // Birth frame
}

static PyObject* py_fish_get_aging_stats(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    
    int total_deaths = fish_get_total_deaths_from_age();
    return PyLong_FromLong(total_deaths);
}

// Fish management functions
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

// Count only currently active fish
static PyObject* py_fish_get_count(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    
    Fish* all_fish = fish_get_all();
    int total_fish = fish_get_count();
    int active_count = 0;
    
    for (int i = 0; i < total_fish; i++) {
        if (all_fish[i].active) {
            active_count++;
        }
    }
    
    return PyLong_FromLong(active_count);
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

// RL system functions
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
    
    return Py_BuildValue("(fffffff)", 
                         fish->rl_inputs[0],  // plant_vector_x
                         fish->rl_inputs[1],  // plant_vector_y
                         fish->rl_inputs[2],  // oxygen_level
                         fish->rl_inputs[3],  // plant_distance
                         fish->rl_inputs[4],  // foreign_fish_vector_x
                         fish->rl_inputs[5],  // foreign_fish_vector_y
                         fish->rl_inputs[6]); // danger_level
}

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
    
    fish->rl_outputs[0] = turn_direction;
    fish->rl_outputs[1] = movement_strength;
    fish->rl_outputs[2] = eat_command;
    
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

// Fish type information including predator status and max age
static PyObject* py_fish_get_type_info(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    FishType* fish_type = fish_get_type(fish->fish_type);
    if (!fish_type) {
        Py_RETURN_NONE;
    }
    
    return Py_BuildValue("(sifii)", 
                         fish_type->name,           // Fish type name
                         fish_type->is_predator,    // Is predator
                         fish_type->danger_level,   // Danger level
                         fish->defecation_count,    // Defecation count
                         fish_type->max_age);       // Max age in frames
}

// Neural network inheritance support
static PyObject* py_fish_get_parent_for_inheritance(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    
    int parent_id = fish_get_parent_for_inheritance();
    return PyLong_FromLong(parent_id);
}

static PyObject* py_fish_is_reproduction_pending(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    
    int pending = fish_is_reproduction_pending();
    return PyLong_FromLong(pending);
}

// Predator-prey system statistics
static PyObject* py_fish_get_predator_stats(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    return Py_BuildValue("(iii)", 
                         fish->eating_cooldown,
                         fish->target_fish_id,
                         fish->defecation_count);
}

static PyObject* py_get_world_bounds(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    return Py_BuildValue("(ffff)", WORLD_LEFT, WORLD_TOP, WORLD_RIGHT, WORLD_BOTTOM);
}

// Nutrition cycle data
static PyObject* py_get_nutrition_balance(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    float fish_consumed = fish_get_total_nutrition_consumed();
    float fish_defecated = fish_get_total_nutrition_defecated();
    float env_total = plants_get_total_environmental_nutrition();
    
    return Py_BuildValue("(fff)", fish_consumed, fish_defecated, env_total);
}

// RL system configuration
static PyObject* py_get_rl_info(PyObject* self, PyObject* args) {
    (void)self;
    (void)args;
    return Py_BuildValue("(ii)", RL_INPUT_SIZE, RL_OUTPUT_SIZE);
}

// Legacy compatibility functions for older scripts
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

static PyObject* py_fish_get_oxygen_level(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
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

// Legacy stub functions for compatibility
static PyObject* py_fish_eat_nearby_plants(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    Py_RETURN_NONE;
}

static PyObject* py_fish_set_movement_force(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    Py_RETURN_NONE;
}

static PyObject* py_fish_apply_rl_action(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    Py_RETURN_NONE;
}

static PyObject* py_fish_get_vision_ray(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    return PyFloat_FromDouble(1.0);
}

static PyObject* py_fish_get_nutrition_ray(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    return PyFloat_FromDouble(0.0);
}

static PyObject* py_get_vision_info(PyObject* self, PyObject* args) {
    (void)self; (void)args;
    return Py_BuildValue("(ii)", 12, 12);
}

// Python method definitions table
static PyMethodDef SimulationMethods[] = {
    {"fish_add", py_fish_add, METH_VARARGS, "Add a fish to the simulation"},
    {"fish_get_count", py_fish_get_count, METH_NOARGS, "Get current active fish count"},
    {"fish_get_position", py_fish_get_position, METH_VARARGS, "Get fish position"},
    {"fish_get_heading", py_fish_get_heading, METH_VARARGS, "Get fish heading in radians"},
    {"fish_get_rl_inputs", py_fish_get_rl_inputs, METH_VARARGS, "Get RL inputs (7 inputs)"},
    {"fish_set_rl_outputs", py_fish_set_rl_outputs, METH_VARARGS, "Set RL outputs (3 outputs)"},
    {"fish_get_last_reward", py_fish_get_last_reward, METH_VARARGS, "Get fish last reward"},
    {"fish_get_stomach_contents", py_fish_get_stomach_contents, METH_VARARGS, "Get fish stomach contents"},
    {"fish_is_eating", py_fish_is_eating, METH_VARARGS, "Check if fish is in eating mode"},
    {"fish_get_type_count", py_fish_get_type_count, METH_NOARGS, "Get fish type count"},
    {"fish_get_type_info", py_fish_get_type_info, METH_VARARGS, "Get fish type info with max_age"},
    {"fish_get_predator_stats", py_fish_get_predator_stats, METH_VARARGS, "Get predator stats"},
    
    // Neural network inheritance functions
    {"fish_get_parent_for_inheritance", py_fish_get_parent_for_inheritance, METH_NOARGS, "Get parent fish ID for NN inheritance"},
    {"fish_is_reproduction_pending", py_fish_is_reproduction_pending, METH_NOARGS, "Check if reproduction notification pending"},
    
    // Aging system functions
    {"fish_get_age_info", py_fish_get_age_info, METH_VARARGS, "Get fish age info (age, max_age, ratio, birth_frame)"},
    {"fish_get_aging_stats", py_fish_get_aging_stats, METH_NOARGS, "Get total deaths from aging"},
    
    // Temperature control functions
    {"temperature_get_current", py_temperature_get_current, METH_NOARGS, "Get current temperature in Celsius"},
    {"temperature_set_current", py_temperature_set_current, METH_VARARGS, "Set current temperature in Celsius"},
    {"temperature_get_bleached_count", py_temperature_get_bleached_count, METH_NOARGS, "Get count of bleached coral nodes"},
    
    // Statistics functions
    {"get_plant_node_count", py_get_plant_node_count, METH_NOARGS, "Get total active plant nodes"},
    {"get_total_environmental_nutrition", py_get_total_environmental_nutrition, METH_NOARGS, "Get total environmental nutrition"},
    
    {"get_world_bounds", py_get_world_bounds, METH_NOARGS, "Get world boundaries"},
    {"get_nutrition_balance", py_get_nutrition_balance, METH_NOARGS, "Get nutrition cycle balance"},
    {"get_rl_info", py_get_rl_info, METH_NOARGS, "Get RL system info (7 inputs, 3 outputs)"},
    
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
    "Marine ecosystem simulation API with neural network support",
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
    
    printf("Python API initialized with neural network support\n");
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
            printf("Neural network script loaded successfully\n");
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