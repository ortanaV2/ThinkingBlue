#include <Python.h>
#include <stdio.h>

#include "types.h"
#include "python_api.h"
#include "fish.h"
#include "simulation.h"

static PyObject* g_python_module = NULL;
static PyObject* g_update_function = NULL;

// Python C API functions that will be available to Python scripts

static PyObject* py_fish_add(PyObject* self, PyObject* args) {
    (void)self; // Suppress unused parameter warning
    float x, y;
    int fish_type;
    
    if (!PyArg_ParseTuple(args, "ffi", &x, &y, &fish_type)) {
        return NULL;
    }
    
    int fish_id = fish_add(x, y, fish_type);
    return PyLong_FromLong(fish_id);
}

static PyObject* py_fish_set_movement_force(PyObject* self, PyObject* args) {
    (void)self; // Suppress unused parameter warning
    int fish_id;
    float force_x, force_y;
    
    if (!PyArg_ParseTuple(args, "iff", &fish_id, &force_x, &force_y)) {
        return NULL;
    }
    
    fish_set_movement_force(fish_id, force_x, force_y);
    Py_RETURN_NONE;
}

static PyObject* py_fish_get_count(PyObject* self, PyObject* args) {
    (void)self; // Suppress unused parameter warning
    (void)args; // Suppress unused parameter warning
    return PyLong_FromLong(fish_get_count());
}

static PyObject* py_fish_get_position(PyObject* self, PyObject* args) {
    (void)self; // Suppress unused parameter warning
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
    (void)self; // Suppress unused parameter warning
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

static PyObject* py_fish_get_type_count(PyObject* self, PyObject* args) {
    (void)self; // Suppress unused parameter warning
    (void)args; // Suppress unused parameter warning
    return PyLong_FromLong(fish_get_type_count());
}

static PyObject* py_get_world_bounds(PyObject* self, PyObject* args) {
    (void)self; // Suppress unused parameter warning
    (void)args; // Suppress unused parameter warning
    return Py_BuildValue("(ffff)", WORLD_LEFT, WORLD_TOP, WORLD_RIGHT, WORLD_BOTTOM);
}

// Method definitions for the Python module
static PyMethodDef SimulationMethods[] = {
    {"fish_add", py_fish_add, METH_VARARGS, "Add a fish to the simulation"},
    {"fish_set_movement_force", py_fish_set_movement_force, METH_VARARGS, "Set fish movement force"},
    {"fish_get_count", py_fish_get_count, METH_NOARGS, "Get total fish count"},
    {"fish_get_position", py_fish_get_position, METH_VARARGS, "Get fish position"},
    {"fish_get_velocity", py_fish_get_velocity, METH_VARARGS, "Get fish velocity"},
    {"fish_get_type_count", py_fish_get_type_count, METH_NOARGS, "Get fish type count"},
    {"get_world_bounds", py_get_world_bounds, METH_NOARGS, "Get world boundaries"},
    {NULL, NULL, 0, NULL}
};

// Module definition
static struct PyModuleDef simulation_module = {
    PyModuleDef_HEAD_INIT,
    "simulation",                    // m_name
    "Marine ecosystem simulation API", // m_doc
    -1,                             // m_size
    SimulationMethods,              // m_methods
    NULL,                           // m_slots
    NULL,                           // m_traverse
    NULL,                           // m_clear
    NULL                            // m_free
};

// Module initialization function
PyMODINIT_FUNC PyInit_simulation(void) {
    return PyModule_Create(&simulation_module);
}

int python_api_init(void) {
    // Add the simulation module to Python's built-in modules
    if (PyImport_AppendInittab("simulation", PyInit_simulation) == -1) {
        printf("Failed to add simulation module to Python\n");
        return 0;
    }
    
    // Initialize Python interpreter
    Py_Initialize();
    if (!Py_IsInitialized()) {
        printf("Failed to initialize Python interpreter\n");
        return 0;
    }
    
    printf("Python API initialized\n");
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
    
    // Run the Python script
    if (PyRun_SimpleFile(fp, script_path) != 0) {
        printf("Failed to execute Python script: %s\n", script_path);
        fclose(fp);
        return 0;
    }
    
    fclose(fp);
    
    // Try to get the main module and update function
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
    
    // Call the Python update function
    PyObject* result = PyObject_CallObject(g_update_function, NULL);
    
    if (result) {
        Py_DECREF(result);
    } else {
        // Print Python error if any
        if (PyErr_Occurred()) {
            PyErr_Print();
        }
    }
}