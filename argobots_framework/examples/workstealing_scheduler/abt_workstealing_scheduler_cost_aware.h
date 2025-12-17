#pragma once
#include <abt.h>

// Cost-aware work stealing scheduler
void ABT_create_ws_scheds_cost_aware(int num, ABT_pool *pools, ABT_sched *scheds);
