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

// Nutrition cost tracking for fish consumption
float plants_get_nutrition_cost_for_node(int node_id);

// Debug function to track total plant nutrition costs
float plants_get_total_nutrition_cost(void);

#endif // PLANTS_H