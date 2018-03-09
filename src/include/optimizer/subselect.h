/*-------------------------------------------------------------------------
 *
 * subselect.h
 *
<<<<<<< HEAD
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
=======
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
>>>>>>> a4bebdd92624e018108c2610fc3f2c1584b6c687
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/optimizer/subselect.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SUBSELECT_H
#define SUBSELECT_H

#include "nodes/plannodes.h"
#include "nodes/relation.h"

extern void SS_process_ctes(PlannerInfo *root);
extern Node *convert_testexpr(PlannerInfo *root,
				 Node *testexpr,
				 List *subst_nodes);
extern JoinExpr *convert_ANY_sublink_to_join(PlannerInfo *root,
							SubLink *sublink,
							Relids available_rels);
extern Node *convert_EXISTS_sublink_to_join(PlannerInfo *root,
							   SubLink *sublink,
							   bool under_not,
							   Relids available_rels);
extern Node *SS_replace_correlation_vars(PlannerInfo *root, Node *expr);
extern Node *SS_process_sublinks(PlannerInfo *root, Node *expr, bool isQual);
extern void SS_finalize_plan(PlannerInfo *root, Plan *plan,
				 bool attach_initplans);
extern Param *SS_make_initplan_from_plan(PlannerInfo *root, Plan *plan,
					Oid resulttype, int32 resulttypmod, Oid resultcollation);
extern Param *assign_nestloop_param(PlannerInfo *root, Var *var);
extern int	SS_assign_special_param(PlannerInfo *root);


extern bool IsSubqueryCorrelated(Query *sq);
extern bool IsSubqueryMultiLevelCorrelated(Query *sq);

extern List *generate_subquery_vars(PlannerInfo *root, List *tlist,
					   Index varno);

#endif   /* SUBSELECT_H */
