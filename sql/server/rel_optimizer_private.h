/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2022 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "rel_optimizer.h"
#include "rel_rel.h"
#include "sql_mvc.h"

/* relations counts */
typedef struct global_props {
	int cnt[ddl_maxops];
	uint8_t
		instantiate:1,
		needs_mergetable_rewrite:1,
		needs_remote_replica_rewrite:1,
		needs_distinct:1,
		needs_setjoin_rewrite:1,
		opt_level:1; /* 0 run necessary rewriters, 1 run all optimizers */
	uint8_t opt_cycle; /* the optimization cycle number */
} global_props;

extern bool can_split_select(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_push_project_down(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_merge_projects(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_push_project_up(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_split_project(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_remove_redundant_join(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_simplify_math(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_optimize_exps(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_optimize_select_and_joins_bottomup(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_project_reduce_casts(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_optimize_unions_bottomup(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_optimize_projections(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_optimize_joins(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_join_order(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_optimize_semi_and_anti(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_optimize_select_and_joins_topdown(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_optimize_unions_topdown(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_dce(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_push_func_and_select_down(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_push_topn_and_sample_down(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_distinct_project2groupby(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_push_select_up(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_merge_table_rewrite(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));
extern bool can_setjoins_2_joingroupby(visitor *v, global_props *gp) __attribute__((__visibility__("hidden")));

extern sql_rel *rel_split_select(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_push_project_down(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_merge_projects(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_push_project_up(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_split_project(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_remove_redundant_join(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_simplify_math(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_optimize_exps(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_optimize_select_and_joins_bottomup(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_project_reduce_casts(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_optimize_unions_bottomup(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_optimize_projections(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_optimize_joins(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_join_order(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_optimize_semi_and_anti(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_optimize_select_and_joins_topdown(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_optimize_unions_topdown(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_dce(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_push_func_and_select_down(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_push_topn_and_sample_down(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_distinct_project2groupby(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_push_select_up(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_merge_table_rewrite(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));
extern sql_rel *rel_setjoins_2_joingroupby(visitor *v, global_props *gp, sql_rel *rel) __attribute__((__visibility__("hidden")));

extern sql_rel *rel_split_project_(visitor *v, sql_rel *rel, int top) __attribute__((__visibility__("hidden")));
extern sql_exp *exp_push_down_prj(mvc *sql, sql_exp *e, sql_rel *f, sql_rel *t) __attribute__((__visibility__("hidden")));
extern sql_exp *add_exp_too_project(mvc *sql, sql_exp *e, sql_rel *rel) __attribute__((__visibility__("hidden")));

/* these functions are used across diferent optimizers */
extern sql_rel *rel_find_ref(sql_rel *r) __attribute__((__visibility__("hidden")));
extern void rel_rename_exps(mvc *sql, list *exps1, list *exps2) __attribute__((__visibility__("hidden")));
extern atom *reduce_scale(mvc *sql, atom *a) __attribute__((__visibility__("hidden")));
extern int exp_range_overlap(atom *min, atom *max, atom *emin, atom *emax, bool min_exclusive, bool max_exclusive) __attribute__((__visibility__("hidden")));
extern int is_numeric_upcast(sql_exp *e) __attribute__((__visibility__("hidden")));
extern sql_exp *list_exps_uses_exp(list *exps, const char *rname, const char *name) __attribute__((__visibility__("hidden")));
extern sql_exp *exps_uses_exp(list *exps, sql_exp *e) __attribute__((__visibility__("hidden")));
extern int sql_class_base_score(visitor *v, sql_column *c, sql_subtype *t, bool equality_based) __attribute__((__visibility__("hidden")));
