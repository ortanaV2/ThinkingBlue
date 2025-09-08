#ifndef PYTHON_API_H
#define PYTHON_API_H

// Python API system initialization and cleanup
int python_api_init(void);
void python_api_cleanup(void);

// Python script execution
int python_api_run_script(const char* script_path);

// Called each frame to update Python-controlled fish
void python_api_update(void);

#endif // PYTHON_API_H