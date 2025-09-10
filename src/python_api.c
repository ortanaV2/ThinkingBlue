#include <Python.h>
#include <stdio.h>

#include "types.h"
#include "python_api.h"
#include "fish.h"
#include "simulation.h"
#include "nutrition.h"

static PyObject* g_python_module = NULL;
static PyObject* g_update_function = NULL;

// Python C API functions that will be available to Python scripts

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

static PyObject* py_fish_set_movement_force(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    float force_x, force_y;
    
    if (!PyArg_ParseTuple(args, "iff", &fish_id, &force_x, &force_y)) {
        return NULL;
    }
    
    fish_set_movement_force(fish_id, force_x, force_y);
    Py_RETURN_NONE;
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

static PyObject* py_fish_get_energy(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    return PyFloat_FromDouble(fish->energy);
}

static PyObject* py_fish_get_stomach_contents(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    return PyFloat_FromDouble(fish->stomach_contents);
}

static PyObject* py_fish_get_consumed_nutrition(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    Fish* fish = fish_get_by_id(fish_id);
    if (!fish) {
        Py_RETURN_NONE;
    }
    
    return PyFloat_FromDouble(fish->consumed_nutrition);
}

static PyObject* py_fish_eat_nearby_plants(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    fish_eat_nearby_plants(fish_id);
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

static PyObject* py_fish_get_vision_ray(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id, ray_index;
    
    if (!PyArg_ParseTuple(args, "ii", &fish_id, &ray_index)) {
        return NULL;
    }
    
    float ray_value = fish_get_vision_ray(fish_id, ray_index);
    return PyFloat_FromDouble(ray_value);
}

static PyObject* py_fish_get_hunger_level(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    float hunger = fish_get_hunger_level(fish_id);
    return PyFloat_FromDouble(hunger);
}

static PyObject* py_fish_get_saturation_level(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    
    if (!PyArg_ParseTuple(args, "i", &fish_id)) {
        return NULL;
    }
    
    float saturation = fish_get_saturation_level(fish_id);
    return PyFloat_FromDouble(saturation);
}

static PyObject* py_fish_apply_rl_action(PyObject* self, PyObject* args) {
    (void)self;
    int fish_id;
    float turn_action, speed_action;
    
    if (!PyArg_ParseTuple(args, "iff", &fish_id, &turn_action, &speed_action)) {
        return NULL;
    }
    
    fish_apply_rl_action(fish_id, turn_action, speed_action);
    Py_RETURN_NONE;
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

// Method definitions for the Python module
static PyMethodDef SimulationMethods[] = {
    {"fish_add", py_fish_add, METH_VARARGS, "Add a fish to the simulation"},
    {"fish_set_movement_force", py_fish_set_movement_force, METH_VARARGS, "Set fish movement force"},
    {"fish_get_count", py_fish_get_count, METH_NOARGS, "Get total fish count"},
    {"fish_get_position", py_fish_get_position, METH_VARARGS, "Get fish position"},
    {"fish_get_velocity", py_fish_get_velocity, METH_VARARGS, "Get fish velocity"},
    {"fish_get_energy", py_fish_get_energy, METH_VARARGS, "Get fish energy level"},
    {"fish_get_stomach_contents", py_fish_get_stomach_contents, METH_VARARGS, "Get fish stomach contents"},
    {"fish_get_consumed_nutrition", py_fish_get_consumed_nutrition, METH_VARARGS, "Get fish total consumed nutrition"},
    {"fish_eat_nearby_plants", py_fish_eat_nearby_plants, METH_VARARGS, "Make fish eat nearby plants"},
    {"fish_get_last_reward", py_fish_get_last_reward, METH_VARARGS, "Get fish last reward"},
    {"fish_get_vision_ray", py_fish_get_vision_ray, METH_VARARGS, "Get fish vision ray value"},
    {"fish_get_hunger_level", py_fish_get_hunger_level, METH_VARARGS, "Get fish hunger level"},
    {"fish_get_saturation_level", py_fish_get_saturation_level, METH_VARARGS, "Get fish saturation level"},
    {"fish_apply_rl_action", py_fish_apply_rl_action, METH_VARARGS, "Apply RL action to fish"},
    {"fish_get_type_count", py_fish_get_type_count, METH_NOARGS, "Get fish type count"},
    {"get_world_bounds", py_get_world_bounds, METH_NOARGS, "Get world boundaries"},
    {"get_nutrition_balance", py_get_nutrition_balance, METH_NOARGS, "Get nutrition cycle balance"},
    {NULL, NULL, 0, NULL}
};

// Module definition
static struct PyModuleDef simulation_module = {
    PyModuleDef_HEAD_INIT,
    "simulation",
    "Marine ecosystem simulation API with nutrition cycle",
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
    
    printf("Python API initialized with nutrition tracking\n");
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
            printf("Python script loaded successfully with update_fish function\n");
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