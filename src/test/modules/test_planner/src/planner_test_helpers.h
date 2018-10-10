#ifndef PLANNER_TEST_HELPERS_H
#define PLANNER_TEST_HELPERS_H

#include "postgres.h"

#include "catalog/pg_type.h"
#include "optimizer/planner.h"

extern Query *make_query(const char *query_string);
extern Plan *get_first_subplan(PlannedStmt *plannedStmt);
extern TargetEntry *get_first_plan_node_target_entry(PlannedStmt *plannedstmt);

#endif //PLANNER_TEST_HELPERS_H
