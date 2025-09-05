#ifndef PLANTS_H
#define PLANTS_H

#include "types.h"

// Plant configuration management
int plants_load_config(const char* filename);

// Plant growth system
void plants_grow(void);

// Plant type accessors
int plants_get_type_count(void);
PlantType* plants_get_type(int index);

#endif // PLANTS_H