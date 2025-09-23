#ifndef PLANTS_H
#define PLANTS_H

#include "types.h"

// Plant configuration management
int plants_load_config(const char* filename);

// Plant growth system
void plants_grow(void);

// Environmental nutrition management
void plants_add_environmental_nutrition(float amount);

// Plant type accessors
int plants_get_type_count(void);
PlantType* plants_get_type(int index);

// Nutrition system
float plants_get_nutrition_from_node(int node_id);
float plants_get_total_environmental_nutrition(void);
void plants_initialize_nutrition_cost(int node_id, int plant_type);

// Legacy compatibility functions
float plants_get_nutrition_cost_for_node(int node_id);
float plants_get_total_nutrition_cost(void);

#endif // PLANTS_H