/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2022 MonetDB B.V.
 */

#ifndef _REL_OPTIMIZER_H_
#define _REL_OPTIMIZER_H_

#include "sql_relation.h"
#include "sql_mvc.h"

#define NOPTIMIZERS 24

typedef struct {
	int nchanges;
	lng time;
} sql_optimizer_run;

typedef struct {
	sql_optimizer_run optimizers[NOPTIMIZERS];
	sql_rel *rel;
} sql_optimized_query;

extern sql_rel *rel_optimizer(mvc *sql, sql_rel *rel, int instantiate, int value_based_opt, int storage_based_opt);

extern int exp_joins_rels(sql_exp *e, list *rels);

/* WARNING exps_unique doesn't check for duplicate NULL values */
extern int exps_unique(mvc *sql, sql_rel *rel, list *exps);

#endif /*_REL_OPTIMIZER_H_*/
