/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "gdk.h"
#include "gdk_analytic.h"
#include "gdk_calc_private.h"

#define SEGMENT_TREE_FANOUT 16
#define NOTHING /* used for not used optional arguments for aggregate computation */

/* segment_tree is the tree as an array, levels_offset contains the offsets in the tree where which level does start,
   and nlevels contains the number of levels on the current segment tree */
static gdk_return
rebuild_segmentree(oid ncount, oid data_size, void **segment_tree, oid *tree_capacity, oid **levels_offset, oid *levels_capacity, oid *nlevels)
{
	oid next_tree_size = ncount, counter = ncount, *new_levels_offset, next_levels = 1; /* there will be at least one level */
	void *new_segment_tree;

	assert(ncount > 0);
	do { /* compute the next number of levels */
		counter = (oid) ceil((dbl)counter / SEGMENT_TREE_FANOUT);
		next_tree_size += counter;
		next_levels++;
	} while (counter > 1);

	next_tree_size *= data_size;
	if (next_tree_size > *tree_capacity) {
		*tree_capacity = (((next_tree_size) + 1023) & ~1023); /* align to a multiple of 1024 bytes */
		if (!(new_segment_tree = GDKmalloc(*tree_capacity)))
			return GDK_FAIL;
		GDKfree(*segment_tree);
		*segment_tree = new_segment_tree;
	}

	*nlevels = next_levels; /* set the logical size of levels before the physical one */
	next_levels *= sizeof(oid);
	if (next_levels > *levels_capacity) {
		*levels_capacity = (((next_levels) + 1023) & ~1023); /* align to a multiple of 1024 bytes */
		if (!(new_levels_offset = GDKmalloc(*levels_capacity)))
			return GDK_FAIL;
		GDKfree(*levels_offset);
		*levels_offset = new_levels_offset;
	}
	return GDK_SUCCEED;
}

/* segment_tree, levels_offset and nlevels must be already defined. ARG1, ARG2 and ARG3 are to be used by the aggregate */
#define populate_segment_tree(CAST, COUNT, INIT_AGGREGATE, COMPUTE_LEVEL0, COMPUTE_LEVELN, ARG1, ARG2, ARG3) \
	do {	\
		CAST *ctree = (CAST *) segment_tree; \
		CAST *prev_level_begin = ctree; \
		oid level_size = COUNT, tree_offset = 0, current_level = 0; \
	\
		levels_offset[current_level++] = 0; /* first level is trivial */ \
		for (oid pos = 0; pos < level_size; pos += SEGMENT_TREE_FANOUT) { \
			oid end = MIN(level_size, pos + SEGMENT_TREE_FANOUT); \
	\
			for (oid x = pos; x < end; x++) { \
				CAST computed; \
				COMPUTE_LEVEL0(x, ARG1, ARG2, ARG3);	\
				ctree[tree_offset++] = computed; \
			} \
		} \
	\
 		while (current_level < nlevels) { /* for the following levels we have to use the previous level results */  \
		 	oid prev_tree_offset = tree_offset; \
			levels_offset[current_level++] = tree_offset; \
			for (oid pos = 0; pos < level_size; pos += SEGMENT_TREE_FANOUT) { \
				oid begin = pos, end = MIN(level_size, pos + SEGMENT_TREE_FANOUT), width = end - begin; \
				CAST computed; \
	\
				INIT_AGGREGATE(ARG1, ARG2, ARG3);	\
				for (oid x = 0; x < width; x++) \
					COMPUTE_LEVELN(prev_level_begin[x], ARG1, ARG2, ARG3);	\
				ctree[tree_offset++] = computed; \
				prev_level_begin += width; \
			} \
			level_size = tree_offset - prev_tree_offset; \
		} \
	} while (0)

#define compute_on_segment_tree(CAST, START, END, INIT_AGGREGATE, COMPUTE, FINALIZE_AGGREGATE, ARG1, ARG2, ARG3)	\
	do { /* taken from https://www.vldb.org/pvldb/vol8/p1058-leis.pdf */	\
		oid begin = START, tend = END; \
		CAST computed; \
	\
		INIT_AGGREGATE(ARG1, ARG2, ARG3);	\
		for (oid level = 0; level < nlevels; level++) { \
			CAST *tlevel = (CAST *) segment_tree + levels_offset[level]; \
			oid parent_begin = begin / SEGMENT_TREE_FANOUT; \
			oid parent_end = tend / SEGMENT_TREE_FANOUT; \
	\
			if (parent_begin == parent_end) { \
				for (oid pos = begin; pos < tend; pos++) \
					COMPUTE(tlevel[pos], ARG1, ARG2, ARG3);	\
				break; \
			} \
			oid group_begin = parent_begin * SEGMENT_TREE_FANOUT; \
			if (begin != group_begin) { \
				oid limit = group_begin + SEGMENT_TREE_FANOUT; \
				for (oid pos = begin; pos < limit; pos++) \
					COMPUTE(tlevel[pos], ARG1, ARG2, ARG3);	\
				parent_begin++; \
			} \
			oid group_end = parent_end * SEGMENT_TREE_FANOUT; \
			if (tend != group_end) { \
				for (oid pos = group_end; pos < tend; pos++) \
					COMPUTE(tlevel[pos], ARG1, ARG2, ARG3);	\
			} \
			begin = parent_begin; \
			tend = parent_end; \
		} \
		FINALIZE_AGGREGATE(ARG1, ARG2, ARG3); \
	} while (0)

#define NTILE_CALC(TPE, NEXT_VALUE, LNG_HGE, UPCAST, VALIDATION)	\
	do {					\
		TPE j = 0; \
		UPCAST ncnt = (UPCAST) (i - k); \
		for (; k < i; k++, j++) {	\
			TPE val = NEXT_VALUE; \
			if (is_##TPE##_nil(val)) {	\
				has_nils = true;	\
				rb[k] = TPE##_nil;	\
			} else { \
				UPCAST nval = (UPCAST) LNG_HGE; \
				VALIDATION /* validation must come after null check */	\
				if (nval >= ncnt) { \
					rb[k] = j + 1;  \
				} else { \
					UPCAST bsize = ncnt / nval; \
					UPCAST top = ncnt - nval * bsize; \
					UPCAST small = top * (bsize + 1); \
					if ((UPCAST) j < small) \
						rb[k] = (TPE)(1 + j / (bsize + 1)); \
					else \
						rb[k] = (TPE)(1 + top + (j - small) / bsize); \
				} \
			} \
		} \
	} while (0)

#define ANALYTICAL_NTILE_IMP(TPE, NEXT_VALUE, LNG_HGE, UPCAST, VALIDATION)	\
	do {							\
		TPE *restrict rb = (TPE*)Tloc(r, 0);		\
		if (p) {						\
			for (; i < cnt; i++) {			\
				if (np[i]) 			\
					NTILE_CALC(TPE, NEXT_VALUE, LNG_HGE, UPCAST, VALIDATION);\
			}						\
		} 					\
		i = cnt;					\
		NTILE_CALC(TPE, NEXT_VALUE, LNG_HGE, UPCAST, VALIDATION);	\
	} while (0)

#define ANALYTICAL_NTILE_SINGLE_IMP(TPE, LNG_HGE, UPCAST) \
	do {	\
		TPE ntl = *(TPE*) ntile; \
		if (!is_##TPE##_nil(ntl) && ntl < 0) goto invalidntile; \
		ANALYTICAL_NTILE_IMP(TPE, ntl, LNG_HGE, UPCAST, ;); \
	} while (0)

#define ANALYTICAL_NTILE_MULTI_IMP(TPE, LNG_HGE, UPCAST) \
	do {	\
		TPE *restrict nn = (TPE*)Tloc(n, 0);	\
		ANALYTICAL_NTILE_IMP(TPE, nn[k], LNG_HGE, UPCAST, if (val < 0) goto invalidntile;); \
	} while (0)

gdk_return
GDKanalyticalntile(BAT *r, BAT *b, BAT *p, BAT *n, int tpe, const void *restrict ntile)
{
	lng i = 0, k = 0, cnt = (lng) BATcount(b);
	bit *restrict np = p ? Tloc(p, 0) : NULL;
	bool has_nils = false;

	assert((n && !ntile) || (!n && ntile));

	if (ntile) {
		switch (tpe) {
		case TYPE_bte:
			ANALYTICAL_NTILE_SINGLE_IMP(bte, val, BUN);
			break;
		case TYPE_sht:
			ANALYTICAL_NTILE_SINGLE_IMP(sht, val, BUN);
			break;
		case TYPE_int:
			ANALYTICAL_NTILE_SINGLE_IMP(int, val, BUN);
			break;
		case TYPE_lng:
#if SIZEOF_OID == SIZEOF_INT
			ANALYTICAL_NTILE_SINGLE_IMP(lng, val, lng);
#else
			ANALYTICAL_NTILE_SINGLE_IMP(lng, val, BUN);
#endif
			break;
#ifdef HAVE_HGE
		case TYPE_hge:
#if SIZEOF_OID == SIZEOF_INT
			ANALYTICAL_NTILE_SINGLE_IMP(hge, (val > (hge) GDK_int_max) ? GDK_int_max : (lng) val, lng);
#else
			ANALYTICAL_NTILE_SINGLE_IMP(hge, (val > (hge) GDK_lng_max) ? GDK_lng_max : (lng) val, BUN);
#endif
#endif
		default:
			goto nosupport;
		}
	} else {
		switch (tpe) {
		case TYPE_bte:
			ANALYTICAL_NTILE_MULTI_IMP(bte, val, BUN);
			break;
		case TYPE_sht:
			ANALYTICAL_NTILE_MULTI_IMP(sht, val, BUN);
			break;
		case TYPE_int:
			ANALYTICAL_NTILE_MULTI_IMP(int, val, BUN);
			break;
		case TYPE_lng:
#if SIZEOF_OID == SIZEOF_INT
			ANALYTICAL_NTILE_MULTI_IMP(lng, val, lng);
#else
			ANALYTICAL_NTILE_MULTI_IMP(lng, val, BUN);
#endif
			break;
#ifdef HAVE_HGE
		case TYPE_hge:
#if SIZEOF_OID == SIZEOF_INT
			ANALYTICAL_NTILE_MULTI_IMP(hge, (val > (hge) GDK_int_max) ? GDK_int_max : (lng) val, lng);
#else
			ANALYTICAL_NTILE_MULTI_IMP(hge, (val > (hge) GDK_lng_max) ? GDK_lng_max : (lng) val, BUN);
#endif
		break;
#endif
		default:
			goto nosupport;
		}
	}
	BATsetcount(r, BATcount(b));
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
nosupport:
	GDKerror("42000!type %s not supported for the ntile type.\n", ATOMname(tpe));
	return GDK_FAIL;
invalidntile:
	GDKerror("42000!ntile must be greater than zero.\n");
	return GDK_FAIL;
}

#define ANALYTICAL_FIRST_FIXED(TPE)				\
	do {							\
		TPE *bp = (TPE*)Tloc(b, 0), *restrict rb = (TPE*)Tloc(r, 0);	\
		for (; k < cnt; k++) { \
			TPE *bs = bp + start[k], *be = bp + end[k];			\
			TPE curval = (be > bs) ? *bs : TPE##_nil;	\
			rb[k] = curval;				\
			has_nils |= is_##TPE##_nil(curval);		\
		}						\
	} while (0)

gdk_return
GDKanalyticalfirst(BAT *r, BAT *b, BAT *s, BAT *e, int tpe)
{
	bool has_nils = false;
	oid k = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;
	BATiter bpi = bat_iterator(b);
	const void *nil = ATOMnilptr(tpe);
	int (*atomcmp)(const void *, const void *) = ATOMcompare(tpe);

	switch (ATOMbasetype(tpe)) {
	case TYPE_bte:
		ANALYTICAL_FIRST_FIXED(bte);
		break;
	case TYPE_sht:
		ANALYTICAL_FIRST_FIXED(sht);
		break;
	case TYPE_int:
		ANALYTICAL_FIRST_FIXED(int);
		break;
	case TYPE_lng:
		ANALYTICAL_FIRST_FIXED(lng);
		break;
#ifdef HAVE_HGE
	case TYPE_hge:
		ANALYTICAL_FIRST_FIXED(hge);
		break;
#endif
	case TYPE_flt:
		ANALYTICAL_FIRST_FIXED(flt);
		break;
	case TYPE_dbl:
		ANALYTICAL_FIRST_FIXED(dbl);
		break;
	default:{
		for (; k < cnt; k++) {
			const void *curval = (end[k] > start[k]) ? BUNtail(bpi, start[k]) : nil;
			if (tfastins_nocheckVAR(r, k, curval, Tsize(r)) != GDK_SUCCEED)
				return GDK_FAIL;
			has_nils |= atomcmp(curval, nil) == 0;
		}
	}
	}

	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
}

#define ANALYTICAL_LAST_FIXED(TPE)				\
	do {							\
		TPE *bp = (TPE*)Tloc(b, 0), *restrict rb = (TPE*)Tloc(r, 0);	\
		for (; k < cnt; k++) { \
			TPE *bs = bp + start[k], *be = bp + end[k];			\
			TPE curval = (be > bs) ? *(be - 1) : TPE##_nil;	\
			rb[k] = curval;				\
			has_nils |= is_##TPE##_nil(curval);		\
		}						\
	} while (0)

gdk_return
GDKanalyticallast(BAT *r, BAT *b, BAT *s, BAT *e, int tpe)
{
	bool has_nils = false;
	oid k = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;
	BATiter bpi = bat_iterator(b);
	const void *nil = ATOMnilptr(tpe);
	int (*atomcmp)(const void *, const void *) = ATOMcompare(tpe);

	switch (ATOMbasetype(tpe)) {
	case TYPE_bte:
		ANALYTICAL_LAST_FIXED(bte);
		break;
	case TYPE_sht:
		ANALYTICAL_LAST_FIXED(sht);
		break;
	case TYPE_int:
		ANALYTICAL_LAST_FIXED(int);
		break;
	case TYPE_lng:
		ANALYTICAL_LAST_FIXED(lng);
		break;
#ifdef HAVE_HGE
	case TYPE_hge:
		ANALYTICAL_LAST_FIXED(hge);
		break;
#endif
	case TYPE_flt:
		ANALYTICAL_LAST_FIXED(flt);
		break;
	case TYPE_dbl:
		ANALYTICAL_LAST_FIXED(dbl);
		break;
	default:{
		for (; k < cnt; k++) {
			const void *curval = (end[k] > start[k]) ? BUNtail(bpi, end[k] - 1) : nil;
			if (tfastins_nocheckVAR(r, k, curval, Tsize(r)) != GDK_SUCCEED)
				return GDK_FAIL;
			has_nils |= atomcmp(curval, nil) == 0;
		}
	}
	}
	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
}

#define ANALYTICAL_NTHVALUE_IMP_SINGLE_FIXED(TPE)			\
	do {								\
		TPE *bp = (TPE*)Tloc(b, 0), *restrict rb = (TPE*)Tloc(r, 0);	\
		if (is_lng_nil(nth)) {					\
			has_nils = true;				\
			for (; k < cnt; k++)			\
				rb[k] = TPE##_nil;			\
		} else {						\
			nth--;						\
			for (; k < cnt; k++) {			\
				TPE *bs = bp + start[k];			\
				TPE *be = bp + end[k];			\
				TPE curval = (be > bs && nth < (lng)(end[k] - start[k])) ? *(bs + nth) : TPE##_nil; \
				rb[k] = curval;				\
				has_nils |= is_##TPE##_nil(curval);	\
			}						\
		}							\
	} while (0)

#define ANALYTICAL_NTHVALUE_IMP_MULTI_FIXED(TPE)			\
	do {								\
		TPE *bp = (TPE*)Tloc(b, 0), curval, *restrict rb = (TPE*)Tloc(r, 0);	\
		for (; k < cnt; k++) {				\
			lng lnth = tp[k];				\
			TPE *bs = bp + start[k];				\
			TPE *be = bp + end[k];				\
			if (!is_lng_nil(lnth) && lnth <= 0) goto invalidnth;	\
			if (is_lng_nil(lnth) || be <= bs || lnth - 1 > (lng)(end[k] - start[k])) { \
				curval = TPE##_nil;	\
				has_nils = true;	\
			} else {						\
				curval = *(bs + lnth - 1);		\
				has_nils |= is_##TPE##_nil(curval);	\
			}	\
			rb[k] = curval;	\
		}							\
	} while (0)

gdk_return
GDKanalyticalnthvalue(BAT *r, BAT *b, BAT *s, BAT *e, BAT *t, lng *pnth, int tpe)
{
	bool has_nils = false;
	oid k = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;
	lng nth = pnth ? *pnth : 0, *restrict tp = t ? (lng*)Tloc(t, 0) : NULL;
	BATiter bpi = bat_iterator(b);
	const void *nil = ATOMnilptr(tpe);
	int (*atomcmp)(const void *, const void *) = ATOMcompare(tpe);

	if (t && t->ttype != TYPE_lng)
		goto nosupport;

	if (t) {
		switch (ATOMbasetype(tpe)) {
		case TYPE_bte:
			ANALYTICAL_NTHVALUE_IMP_MULTI_FIXED(bte);
			break;
		case TYPE_sht:
			ANALYTICAL_NTHVALUE_IMP_MULTI_FIXED(sht);
			break;
		case TYPE_int:
			ANALYTICAL_NTHVALUE_IMP_MULTI_FIXED(int);
			break;
		case TYPE_lng:
			ANALYTICAL_NTHVALUE_IMP_MULTI_FIXED(lng);
			break;
#ifdef HAVE_HGE
		case TYPE_hge:
			ANALYTICAL_NTHVALUE_IMP_MULTI_FIXED(hge);
			break;
#endif
		case TYPE_flt:
			ANALYTICAL_NTHVALUE_IMP_MULTI_FIXED(flt);
			break;
		case TYPE_dbl:
			ANALYTICAL_NTHVALUE_IMP_MULTI_FIXED(dbl);
			break;
		default:{
			const void *curval = nil;
			for (; k < cnt; k++) {
				lng lnth = tp[k];
				if (!is_lng_nil(nth) && nth <= 0) goto invalidnth;
				if (is_lng_nil(lnth) || end[k] <= start[k] || lnth - 1 > (lng)(end[k] - start[k])) {
					curval = (void *) nil;
					has_nils = true;
				} else {
					curval = BUNtail(bpi, start[k] + (oid)(lnth - 1));
					has_nils |= atomcmp(curval, nil) == 0;
				}
				if (tfastins_nocheckVAR(r, k, curval, Tsize(r)) != GDK_SUCCEED)
					return GDK_FAIL;
			}
		}
		}
	} else {
		if (!is_lng_nil(nth) && nth <= 0)
			goto invalidnth;
		switch (ATOMbasetype(tpe)) {
		case TYPE_bte:
			ANALYTICAL_NTHVALUE_IMP_SINGLE_FIXED(bte);
			break;
		case TYPE_sht:
			ANALYTICAL_NTHVALUE_IMP_SINGLE_FIXED(sht);
			break;
		case TYPE_int:
			ANALYTICAL_NTHVALUE_IMP_SINGLE_FIXED(int);
			break;
		case TYPE_lng:
			ANALYTICAL_NTHVALUE_IMP_SINGLE_FIXED(lng);
			break;
#ifdef HAVE_HGE
		case TYPE_hge:
			ANALYTICAL_NTHVALUE_IMP_SINGLE_FIXED(hge);
			break;
#endif
		case TYPE_flt:
			ANALYTICAL_NTHVALUE_IMP_SINGLE_FIXED(flt);
			break;
		case TYPE_dbl:
			ANALYTICAL_NTHVALUE_IMP_SINGLE_FIXED(dbl);
			break;
		default:{
			if (is_lng_nil(nth)) {
				has_nils = true;
				for (; k < cnt; k++)
					if (tfastins_nocheckVAR(r, k, nil, Tsize(r)) != GDK_SUCCEED)
						return GDK_FAIL;
			} else {
				nth--;
				for (; k < cnt; k++) {
					const void *curval = (end[k] > start[k] && nth < (lng)(end[k] - start[k])) ? BUNtail(bpi, start[k] + (oid) nth) : nil;
					if (tfastins_nocheckVAR(r, k, curval, Tsize(r)) != GDK_SUCCEED)
						return GDK_FAIL;
					has_nils |= atomcmp(curval, nil) == 0;
				}
			}
		}
		}
	}

	BATsetcount(r, BATcount(b));
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
nosupport:
	GDKerror("42000!type %s not supported for the nth_value.\n", ATOMname(t->ttype));
	return GDK_FAIL;
invalidnth:
	GDKerror("42000!nth_value must be greater than zero.\n");
	return GDK_FAIL;
}

#define ANALYTICAL_LAG_CALC(TPE)				\
	do {							\
		for (i = 0; i < lag && rb < rp; i++, rb++)	\
			*rb = def;				\
		has_nils |= (lag > 0 && is_##TPE##_nil(def));	\
		for (; rb < rp; rb++, bp++) {			\
			next = *bp;				\
			*rb = next;				\
			has_nils |= is_##TPE##_nil(next);		\
		}						\
	} while (0)

#define ANALYTICAL_LAG_IMP(TPE)						\
	do {								\
		TPE *rp, *rb, *bp, *nbp, *rend,				\
			def = *((TPE *) default_value), next;		\
		bp = (TPE*)Tloc(b, 0);					\
		rb = rp = (TPE*)Tloc(r, 0);				\
		rend = rb + cnt;					\
		if (lag == BUN_NONE) {					\
			has_nils = true;				\
			for (; rb < rend; rb++)				\
				*rb = TPE##_nil;			\
		} else if (p) {						\
			pnp = np = (bit*)Tloc(p, 0);			\
			end = np + cnt;					\
			for (; np < end; np++) {			\
				if (*np) {				\
					ncnt = (np - pnp);		\
					rp += ncnt;			\
					nbp = bp + ncnt;		\
					ANALYTICAL_LAG_CALC(TPE);	\
					bp = nbp;			\
					pnp = np;			\
				}					\
			}						\
			rp += (np - pnp);				\
			ANALYTICAL_LAG_CALC(TPE);			\
		} else {						\
			rp += cnt;					\
			ANALYTICAL_LAG_CALC(TPE);			\
		}							\
	} while (0)

#define ANALYTICAL_LAG_OTHERS						\
	do {								\
		for (i = 0; i < lag && k < j; i++, k++) {		\
			if (BUNappend(r, default_value, false) != GDK_SUCCEED) \
				return GDK_FAIL;			\
		}							\
		has_nils |= (lag > 0 && atomcmp(default_value, nil) == 0);	\
		for (l = k - lag; k < j; k++, l++) {			\
			curval = BUNtail(bpi, l);			\
			if (BUNappend(r, curval, false) != GDK_SUCCEED)	\
				return GDK_FAIL;			\
			has_nils |= atomcmp(curval, nil) == 0;			\
		}	\
	} while (0)

gdk_return
GDKanalyticallag(BAT *r, BAT *b, BAT *p, BUN lag, const void *restrict default_value, int tpe)
{
	int (*atomcmp) (const void *, const void *);
	const void *restrict nil;
	BUN i = 0, j = 0, k = 0, l = 0, ncnt, cnt = BATcount(b);
	bit *np, *pnp, *end;
	bool has_nils = false;

	assert(default_value);

	switch (ATOMbasetype(tpe)) {
	case TYPE_bte:
		ANALYTICAL_LAG_IMP(bte);
		break;
	case TYPE_sht:
		ANALYTICAL_LAG_IMP(sht);
		break;
	case TYPE_int:
		ANALYTICAL_LAG_IMP(int);
		break;
	case TYPE_lng:
		ANALYTICAL_LAG_IMP(lng);
		break;
#ifdef HAVE_HGE
	case TYPE_hge:
		ANALYTICAL_LAG_IMP(hge);
		break;
#endif
	case TYPE_flt:
		ANALYTICAL_LAG_IMP(flt);
		break;
	case TYPE_dbl:
		ANALYTICAL_LAG_IMP(dbl);
		break;
	default:{
		BATiter bpi = bat_iterator(b);
		const void *restrict curval;
		nil = ATOMnilptr(tpe);
		atomcmp = ATOMcompare(tpe);
		if (lag == BUN_NONE) {
			has_nils = true;
			for (j = 0; j < cnt; j++) {
				if (BUNappend(r, nil, false) != GDK_SUCCEED)
					return GDK_FAIL;
			}
		} else if (p) {
			pnp = np = (bit *) Tloc(p, 0);
			end = np + cnt;
			for (; np < end; np++) {
				if (*np) {
					j += (np - pnp);
					ANALYTICAL_LAG_OTHERS;
					pnp = np;
				}
			}
			j += (np - pnp);
			ANALYTICAL_LAG_OTHERS;
		} else {
			j += cnt;
			ANALYTICAL_LAG_OTHERS;
		}
	}
	}
	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
}

#define LEAD_CALC(TPE)						\
	do {							\
		if (lead < ncnt) {				\
			bp += lead;				\
			l = ncnt - lead;			\
			for (i = 0; i < l; i++, rb++, bp++) {	\
				next = *bp;			\
				*rb = next;			\
				has_nils |= is_##TPE##_nil(next);	\
			}					\
		} else {					\
			bp += ncnt;				\
		}						\
		for (;rb < rp; rb++)				\
			*rb = def;				\
		has_nils |= (lead > 0 && is_##TPE##_nil(def));	\
	} while (0)

#define ANALYTICAL_LEAD_IMP(TPE)				\
	do {							\
		TPE *rp, *rb, *bp, *rend,			\
			def = *((TPE *) default_value), next;	\
		bp = (TPE*)Tloc(b, 0);				\
		rb = rp = (TPE*)Tloc(r, 0);			\
		rend = rb + cnt;				\
		if (lead == BUN_NONE) {				\
			has_nils = true;			\
			for (; rb < rend; rb++)			\
				*rb = TPE##_nil;		\
		} else if (p) {					\
			pnp = np = (bit*)Tloc(p, 0);		\
			end = np + cnt;				\
			for (; np < end; np++) {		\
				if (*np) {			\
					ncnt = (np - pnp);	\
					rp += ncnt;		\
					LEAD_CALC(TPE);		\
					pnp = np;		\
				}				\
			}					\
			ncnt = (np - pnp);			\
			rp += ncnt;				\
			LEAD_CALC(TPE);				\
		} else {					\
			ncnt = cnt;				\
			rp += ncnt;				\
			LEAD_CALC(TPE);				\
		}						\
	} while (0)

#define ANALYTICAL_LEAD_OTHERS						\
	do {								\
		j += ncnt;						\
		if (lead < ncnt) {					\
			m = ncnt - lead;				\
			for (i = 0,n = k + lead; i < m; i++, n++) {	\
				curval = BUNtail(bpi, n);		\
				if (BUNappend(r, curval, false) != GDK_SUCCEED)	\
					return GDK_FAIL;		\
				has_nils |= atomcmp(curval, nil) == 0;		\
			}						\
			k += i;						\
		}							\
		for (; k < j; k++) {					\
			if (BUNappend(r, default_value, false) != GDK_SUCCEED) \
				return GDK_FAIL;			\
		}							\
		has_nils |= (lead > 0 && atomcmp(default_value, nil) == 0);	\
	} while (0)

gdk_return
GDKanalyticallead(BAT *r, BAT *b, BAT *p, BUN lead, const void *restrict default_value, int tpe)
{
	int (*atomcmp) (const void *, const void *);
	const void *restrict nil;
	BUN i = 0, j = 0, k = 0, l = 0, ncnt, cnt = BATcount(b);
	bit *np, *pnp, *end;
	bool has_nils = false;

	assert(default_value);

	switch (ATOMbasetype(tpe)) {
	case TYPE_bte:
		ANALYTICAL_LEAD_IMP(bte);
		break;
	case TYPE_sht:
		ANALYTICAL_LEAD_IMP(sht);
		break;
	case TYPE_int:
		ANALYTICAL_LEAD_IMP(int);
		break;
	case TYPE_lng:
		ANALYTICAL_LEAD_IMP(lng);
		break;
#ifdef HAVE_HGE
	case TYPE_hge:
		ANALYTICAL_LEAD_IMP(hge);
		break;
#endif
	case TYPE_flt:
		ANALYTICAL_LEAD_IMP(flt);
		break;
	case TYPE_dbl:
		ANALYTICAL_LEAD_IMP(dbl);
		break;
	default:{
		BUN m = 0, n = 0;
		BATiter bpi = bat_iterator(b);
		const void *restrict curval;
		nil = ATOMnilptr(tpe);
		atomcmp = ATOMcompare(tpe);
		if (lead == BUN_NONE) {
			has_nils = true;
			for (j = 0; j < cnt; j++) {
				if (BUNappend(r, nil, false) != GDK_SUCCEED)
					return GDK_FAIL;
			}
		} else if (p) {
			pnp = np = (bit *) Tloc(p, 0);
			end = np + cnt;
			for (; np < end; np++) {
				if (*np) {
					ncnt = (np - pnp);
					ANALYTICAL_LEAD_OTHERS;
					pnp = np;
				}
			}
			ncnt = (np - pnp);
			ANALYTICAL_LEAD_OTHERS;
		} else {
			ncnt = cnt;
			ANALYTICAL_LEAD_OTHERS;
		}
	}
	}
	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
}

#define ANALYTICAL_MIN_MAX_CALC_FIXED_UNBOUNDED_TILL_CURRENT_ROW(TPE, MIN_MAX)	\
	do { \
		TPE curval = TPE##_nil;	\
		for (; k < i;) { \
			j = k; \
			do {	\
				if (!is_##TPE##_nil(bp[k])) {		\
					if (is_##TPE##_nil(curval))	\
						curval = bp[k];	\
					else				\
						curval = MIN_MAX(bp[k], curval); \
				}					\
				k++; \
			} while (k < i && !op[k]);	\
			for (; j < k; j++) \
				rb[j] = curval; \
			has_nils |= is_##TPE##_nil(curval); \
		} \
	} while (0)

#define ANALYTICAL_MIN_MAX_CALC_FIXED_CURRENT_ROW_TILL_UNBOUNDED(TPE, MIN_MAX)	\
	do { \
		TPE curval = TPE##_nil;	\
		l = i - 1; \
		for (j = l; ; j--) { \
			if (!is_##TPE##_nil(bp[j])) {	\
				if (is_##TPE##_nil(curval))	\
					curval = bp[j];	\
				else				\
					curval = MIN_MAX(bp[j], curval); \
			}	\
			if (op[j] || j == k) {	\
				for (; ; l--) { \
					rb[l] = curval;	\
					if (l == j)	\
						break;	\
				} \
				has_nils |= is_##TPE##_nil(curval); \
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		k = i; \
	} while (0)

#define ANALYTICAL_MIN_MAX_CALC_FIXED_ALL_ROWS(TPE, MIN_MAX)	\
	do { \
		TPE curval = TPE##_nil; \
		for (j = k; j < i; j++) { \
			TPE v = bp[j];				\
			if (!is_##TPE##_nil(v)) {		\
				if (is_##TPE##_nil(curval))	\
					curval = v;	\
				else				\
					curval = MIN_MAX(v, curval); \
			}					\
		} \
		for (; k < i; k++) \
			rb[k] = curval; \
		has_nils |= is_##TPE##_nil(curval); \
	} while (0)

#define ANALYTICAL_MIN_MAX_CALC_FIXED_CURRENT_ROW(TPE, MIN_MAX)	\
	do { \
		for (; k < i; k++) { \
			TPE v = bp[k]; \
			rb[k] = v; \
			has_nils |= is_##TPE##_nil(v); \
		} \
	} while (0)

#define INIT_AGGREGATE_MIN_MAX_FIXED(TPE, MIN_MAX, NOTHING) \
	do { \
		computed = TPE##_nil; \
	} while (0)
#define COMPUTE_LEVEL0_MIN_MAX_FIXED(X, TPE, MIN_MAX, NOTHING) \
	do { \
		computed = bp[j + X]; \
	} while (0)
#define COMPUTE_LEVELN_MIN_MAX_FIXED(VAL, TPE, MIN_MAX, NOTHING) \
	do { \
		if (!is_##TPE##_nil(VAL)) {	\
			if (is_##TPE##_nil(computed))	\
				computed = VAL;	\
			else				\
				computed = MIN_MAX(computed, VAL); \
		} \
	} while (0)
#define FINALIZE_AGGREGATE_MIN_MAX_FIXED(TPE, MIN_MAX, NOTHING) \
	do { \
		rb[k] = computed;	\
		has_nils |= is_##TPE##_nil(computed); \
	} while (0)
#define ANALYTICAL_MIN_MAX_CALC_FIXED_OTHERS(TPE, MIN_MAX)	\
	do { \
		oid ncount = i - k; \
		if ((res = rebuild_segmentree(ncount, sizeof(TPE), &segment_tree, &tree_capacity, &levels_offset, &levels_capacity, &nlevels)) != GDK_SUCCEED) \
			goto cleanup; \
		populate_segment_tree(TPE, ncount, INIT_AGGREGATE_MIN_MAX_FIXED, COMPUTE_LEVEL0_MIN_MAX_FIXED, COMPUTE_LEVELN_MIN_MAX_FIXED, TPE, MIN_MAX, NOTHING); \
		for (; k < i; k++) \
			compute_on_segment_tree(TPE, start[k] - j, end[k] - j, INIT_AGGREGATE_MIN_MAX_FIXED, COMPUTE_LEVELN_MIN_MAX_FIXED, FINALIZE_AGGREGATE_MIN_MAX_FIXED, TPE, MIN_MAX, NOTHING); \
		j = k; \
	} while (0)

#define ANALYTICAL_MIN_MAX_CALC_VARSIZED_UNBOUNDED_TILL_CURRENT_ROW(GT_LT)	\
	do { \
		const void *curval = nil;	\
		for (; k < i;) { \
			j = k; \
			do {	\
				void *next = BUNtail(bpi, k); \
				if (atomcmp(next, nil) != 0) {		\
					if (atomcmp(curval, nil) == 0)	\
						curval = next;		\
					else				\
						curval = atomcmp(next, curval) GT_LT 0 ? curval : next; \
				}					\
				k++; \
			} while (k < i && !op[k]);	\
			for (; j < k; j++) \
				if (tfastins_nocheckVAR(r, j, curval, Tsize(r)) != GDK_SUCCEED) \
					return GDK_FAIL; \
			has_nils |= atomcmp(curval, nil) == 0;		\
		} \
	} while (0)

#define ANALYTICAL_MIN_MAX_CALC_VARSIZED_CURRENT_ROW_TILL_UNBOUNDED(GT_LT)	\
	do { \
		const void *curval = nil;	\
		l = i - 1; \
		for (j = l; ; j--) { \
			void *next = BUNtail(bpi, j); \
			if (atomcmp(next, nil) != 0) {		\
				if (atomcmp(curval, nil) == 0)	\
					curval = next;		\
				else				\
					curval = atomcmp(next, curval) GT_LT 0 ? curval : next; \
			}					\
			if (op[j] || j == k) {	\
				for (; ; l--) { \
					if (tfastins_nocheckVAR(r, l, curval, Tsize(r)) != GDK_SUCCEED) \
						return GDK_FAIL; \
					if (l == j)	\
						break;	\
				} \
				has_nils |= atomcmp(curval, nil) == 0;		\
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		k = i; \
	} while (0)

#define ANALYTICAL_MIN_MAX_CALC_VARSIZED_ALL_ROWS(GT_LT)	\
	do { \
		void *curval = (void*) nil; \
		for (j = k; j < i; j++) { \
			void *next = BUNtail(bpi, j);	\
			if (atomcmp(next, nil) != 0) {		\
				if (atomcmp(curval, nil) == 0)	\
					curval = next;		\
				else				\
					curval = atomcmp(next, curval) GT_LT 0 ? curval : next; \
			}					\
		} \
		for (; k < i; k++) \
			if (tfastins_nocheckVAR(r, k, curval, Tsize(r)) != GDK_SUCCEED) \
				return GDK_FAIL; \
		has_nils |= atomcmp(curval, nil) == 0;		\
	} while (0)

#define ANALYTICAL_MIN_MAX_CALC_VARSIZED_CURRENT_ROW(GT_LT)	\
	do { \
		for (; k < i; k++) { \
			void *next = BUNtail(bpi, k); \
			if (tfastins_nocheckVAR(r, k, next, Tsize(r)) != GDK_SUCCEED) \
				return GDK_FAIL; \
			has_nils |= atomcmp(next, nil) == 0;		\
		} \
	} while (0)

#define INIT_AGGREGATE_MIN_MAX_VARSIZED(GT_LT, NOTHING1, NOTHING2) \
	do { \
		computed = (void*) nil; \
	} while (0)
#define COMPUTE_LEVEL0_MIN_MAX_VARSIZED(X, GT_LT, NOTHING1, NOTHING2) \
	do { \
		computed = BUNtail(bpi, j + X); \
	} while (0)
#define COMPUTE_LEVELN_MIN_MAX_VARSIZED(VAL, GT_LT, NOTHING1, NOTHING2) \
	do { \
		if (atomcmp(VAL, nil) != 0) {		\
			if (atomcmp(computed, nil) == 0)	\
				computed = VAL;	\
			else		\
				computed = atomcmp(VAL, computed) GT_LT 0 ? computed : VAL; \
		} \
	} while (0)
#define FINALIZE_AGGREGATE_MIN_MAX_VARSIZED(GT_LT, NOTHING1, NOTHING2) \
	do { \
		if ((res = tfastins_nocheckVAR(r, k, computed, Tsize(r))) != GDK_SUCCEED) \
			goto cleanup; \
		has_nils |= atomcmp(computed, nil) == 0;		\
	} while (0)
#define ANALYTICAL_MIN_MAX_CALC_VARSIZED_OTHERS(GT_LT)	\
	do { \
		oid ncount = i - k; \
		if ((res = rebuild_segmentree(ncount, sizeof(void*), &segment_tree, &tree_capacity, &levels_offset, &levels_capacity, &nlevels)) != GDK_SUCCEED) \
			goto cleanup; \
		populate_segment_tree(void*, ncount, INIT_AGGREGATE_MIN_MAX_VARSIZED, COMPUTE_LEVEL0_MIN_MAX_VARSIZED, COMPUTE_LEVELN_MIN_MAX_VARSIZED, GT_LT, NOTHING, NOTHING); \
		for (; k < i; k++) \
			compute_on_segment_tree(void*, start[k] - j, end[k] - j, INIT_AGGREGATE_MIN_MAX_VARSIZED, COMPUTE_LEVELN_MIN_MAX_VARSIZED, FINALIZE_AGGREGATE_MIN_MAX_VARSIZED, GT_LT, NOTHING, NOTHING); \
		j = k; \
	} while (0)

#define ANALYTICAL_MIN_MAX_PARTITIONS(TPE, MIN_MAX, IMP)		\
	do {					\
		TPE *bp = (TPE*)Tloc(b, 0), *restrict rb = (TPE*)Tloc(r, 0); \
		if (p) {					\
			for (; i < cnt; i++) {		\
				if (np[i]) 			\
					ANALYTICAL_MIN_MAX_CALC_FIXED_##IMP(TPE, MIN_MAX); \
			}						\
		}		\
		i = cnt;					\
		ANALYTICAL_MIN_MAX_CALC_FIXED_##IMP(TPE, MIN_MAX);	\
	} while (0)

#ifdef HAVE_HGE
#define ANALYTICAL_MIN_MAX_LIMIT(MIN_MAX, IMP)			\
	case TYPE_hge:					\
		ANALYTICAL_MIN_MAX_PARTITIONS(hge, MIN_MAX, IMP);	\
	break;
#else
#define ANALYTICAL_MIN_MAX_LIMIT(MIN_MAX, IMP)
#endif

#define ANALYTICAL_MIN_MAX_BRANCHES(MIN_MAX, GT_LT, IMP)		\
	do { \
		switch (ATOMbasetype(tpe)) {				\
		case TYPE_bte:							\
			ANALYTICAL_MIN_MAX_PARTITIONS(bte, MIN_MAX, IMP);			\
			break;							\
		case TYPE_sht:							\
			ANALYTICAL_MIN_MAX_PARTITIONS(sht, MIN_MAX, IMP);			\
			break;							\
		case TYPE_int:							\
			ANALYTICAL_MIN_MAX_PARTITIONS(int, MIN_MAX, IMP);			\
			break;							\
		case TYPE_lng:							\
			ANALYTICAL_MIN_MAX_PARTITIONS(lng, MIN_MAX, IMP);			\
			break;							\
			ANALYTICAL_MIN_MAX_LIMIT(MIN_MAX, IMP)				\
		case TYPE_flt:							\
			ANALYTICAL_MIN_MAX_PARTITIONS(flt, MIN_MAX, IMP);			\
			break;							\
		case TYPE_dbl:							\
			ANALYTICAL_MIN_MAX_PARTITIONS(dbl, MIN_MAX, IMP);			\
			break;							\
		default: {							\
			if (p) {						\
				for (; i < cnt; i++) {			\
					if (np[i]) 			\
						ANALYTICAL_MIN_MAX_CALC_VARSIZED_##IMP(GT_LT); \
				}						\
			} 					\
			i = cnt;					\
			ANALYTICAL_MIN_MAX_CALC_VARSIZED_##IMP(GT_LT);	\
		}								\
		}								\
	} while (0)

#define ANALYTICAL_MIN_MAX(OP, MIN_MAX, GT_LT)				\
gdk_return								\
GDKanalytical##OP(BAT *r, BAT *p, BAT *o, BAT *b, BAT *s, BAT *e, int tpe, int frame_type)		\
{									\
	bool has_nils = false;						\
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL, \
		*levels_offset = NULL, tree_capacity = 0, nlevels = 0, levels_capacity = 0;	\
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL; 	\
	BATiter bpi = bat_iterator(b);				\
	const void *nil = ATOMnilptr(tpe);			\
	int (*atomcmp)(const void *, const void *) = ATOMcompare(tpe); \
	void *segment_tree = NULL; \
	gdk_return res = GDK_SUCCEED; \
	\
	if (cnt > 0) {	\
		switch (frame_type) {		\
		case 3: /* unbounded until current row */	{	\
			ANALYTICAL_MIN_MAX_BRANCHES(MIN_MAX, GT_LT, UNBOUNDED_TILL_CURRENT_ROW); \
		} break;		\
		case 4: /* current row until unbounded */	{	\
			ANALYTICAL_MIN_MAX_BRANCHES(MIN_MAX, GT_LT, CURRENT_ROW_TILL_UNBOUNDED); \
		} break;		\
		case 5: /* all rows */	{	\
			ANALYTICAL_MIN_MAX_BRANCHES(MIN_MAX, GT_LT, ALL_ROWS); \
		} break;		\
		case 6: /* current row */ {	\
			ANALYTICAL_MIN_MAX_BRANCHES(MIN_MAX, GT_LT, CURRENT_ROW); \
		} break;		\
		default: {		\
			ANALYTICAL_MIN_MAX_BRANCHES(MIN_MAX, GT_LT, OTHERS); \
		}		\
		}	\
	} \
	\
	BATsetcount(r, cnt);	\
	r->tnonil = !has_nils;	\
	r->tnil = has_nils;		\
cleanup: \
	GDKfree(segment_tree); \
	GDKfree(levels_offset); \
	return res; \
}

ANALYTICAL_MIN_MAX(min, MIN, >)
ANALYTICAL_MIN_MAX(max, MAX, <)

/* Counting no nils for fixed sizes */
#define ANALYTICAL_COUNT_FIXED_UNBOUNDED_TILL_CURRENT_ROW(TPE) \
	do { \
		curval = 0; \
		if (count_all) { \
			for (; k < i;) { \
				j = k; \
				do {	\
					k++; \
				} while (k < i && !op[k]);	\
				curval += k - j; \
				for (; j < k; j++) \
					rb[j] = curval; \
			} \
		} else { \
			for (; k < i;) { \
				j = k; \
				do {	\
					curval += !is_##TPE##_nil(bp[k]); \
					k++; \
				} while (k < i && !op[k]);	\
				for (; j < k; j++) \
					rb[j] = curval; \
			} \
		}	\
	} while (0)

#define ANALYTICAL_COUNT_FIXED_CURRENT_ROW_TILL_UNBOUNDED(TPE) \
	do { \
		curval = 0; \
		l = i - 1; \
		if (count_all) { \
			for (j = l; ; j--) { \
				if (op[j] || j == k) {	\
					curval += l - j + 1; \
					for (; ; l--) { \
						rb[l] = curval; \
						if (l == j)	\
							break;	\
					} \
					if (j == k)	\
						break;	\
					l = j - 1;	\
				}	\
			}	\
		} else { \
			for (j = l; ; j--) { \
				curval += !is_##TPE##_nil(bp[j]); \
				if (op[j] || j == k) {	\
					for (; ; l--) { \
						rb[l] = curval; \
						if (l == j)	\
							break;	\
					} \
					if (j == k)	\
						break;	\
					l = j - 1;	\
				}	\
			}	\
		} \
		k = i; \
	} while (0)

#define ANALYTICAL_COUNT_FIXED_ALL_ROWS(TPE) \
	do { \
		if (count_all) { \
			curval = (lng)(i - k); \
			for (; k < i; k++) \
				rb[k] = curval; \
		} else {	\
			curval = 0; \
			for (; j < i; j++) \
				curval += !is_##TPE##_nil(bp[j]); \
			for (; k < i; k++) \
				rb[k] = curval; \
		}	\
	} while (0)

#define ANALYTICAL_COUNT_FIXED_CURRENT_ROW(TPE) \
	do { \
		if (count_all) { \
			for (; k < i; k++) \
				rb[k] = 1; \
		} else { \
			for (; k < i; k++) \
				rb[k] = !is_##TPE##_nil(bp[k]); \
		} \
	} while (0)

#define INIT_AGGREGATE_COUNT(TPE, NOTHING1, NOTHING2) \
	do { \
		computed = 0; \
	} while (0)
#define COMPUTE_LEVEL0_COUNT_FIXED(X, TPE, NOTHING1, NOTHING2) \
	do { \
		computed = !is_##TPE##_nil(bp[j + X]); \
	} while (0)
#define COMPUTE_LEVELN_COUNT(VAL, NOTHING1, NOTHING2, NOTHING3) \
	do { \
		computed += VAL; \
	} while (0)
#define FINALIZE_AGGREGATE_COUNT(NOTHING1, NOTHING2, NOTHING3) \
	do { \
		rb[k] = computed;	\
	} while (0)
#define ANALYTICAL_COUNT_FIXED_OTHERS(TPE)	\
	do { \
		if (count_all) { /* no segment tree required for the global case (it scales in O(n)) */ \
			for (; k < i; k++) \
				rb[k] = (end[k] > start[k]) ? (lng)(end[k] - start[k]) : 0; \
		} else {	\
			oid ncount = i - k; \
			if ((res = rebuild_segmentree(ncount, sizeof(lng), &segment_tree, &tree_capacity, &levels_offset, &levels_capacity, &nlevels)) != GDK_SUCCEED) \
				goto cleanup; \
			populate_segment_tree(lng, ncount, INIT_AGGREGATE_COUNT, COMPUTE_LEVEL0_COUNT_FIXED, COMPUTE_LEVELN_COUNT, TPE, NOTHING, NOTHING); \
			for (; k < i; k++) \
				compute_on_segment_tree(lng, start[k] - j, end[k] - j, INIT_AGGREGATE_COUNT, COMPUTE_LEVELN_COUNT, FINALIZE_AGGREGATE_COUNT, TPE, NOTHING, NOTHING); \
			j = k; \
		}	\
	} while (0)

/* Counting no nils for other types */
#define ANALYTICAL_COUNT_OTHERS_UNBOUNDED_TILL_CURRENT_ROW \
	do { \
		curval = 0; \
		if (count_all) { \
			for (; k < i;) { \
				j = k; \
				do {	\
					k++; \
				} while (k < i && !op[k]);	\
				curval += k - j; \
				for (; j < k; j++) \
					rb[j] = curval; \
			} \
		} else { \
			for (; k < i; ) { \
				j = k; \
				do {	\
					curval += cmp(BUNtail(bpi, k), nil) != 0; \
					k++; \
				} while (k < i && !op[k]);	\
				for (; j < k; j++) \
					rb[j] = curval; \
			} \
		}	\
	} while (0)

#define ANALYTICAL_COUNT_OTHERS_CURRENT_ROW_TILL_UNBOUNDED \
	do { \
		curval = 0; \
		l = i - 1; \
		if (count_all) { \
			for (j = l; ; j--) { \
				if (op[j] || j == k) {	\
					curval += l - j + 1; \
					for (; ; l--) { \
						rb[l] = curval; \
						if (l == j)	\
							break;	\
					} \
					if (j == k)	\
						break;	\
					l = j - 1;	\
				}	\
			}	\
		} else { \
			for (j = l; ; j--) { \
				curval += cmp(BUNtail(bpi, j), nil) != 0;	\
				if (op[j] || j == k) {	\
					for (; ; l--) { \
						rb[l] = curval; \
						if (l == j)	\
							break;	\
					} \
					if (j == k)	\
						break;	\
					l = j - 1;	\
				}	\
			}	\
		} \
		k = i; \
	} while (0)

#define ANALYTICAL_COUNT_OTHERS_ALL_ROWS	\
	do { \
		curval = 0; \
		if (count_all) { \
			curval = (lng)(i - k); \
		} else {	\
			for (; j < i; j++) \
				curval += cmp(BUNtail(bpi, j), nil) != 0; \
		}	\
		for (; k < i; k++) \
			rb[k] = curval; \
	} while (0)

#define ANALYTICAL_COUNT_OTHERS_CURRENT_ROW	\
	do { \
		if (count_all) { \
			for (; k < i; k++) \
				rb[k] = 1; \
		} else { \
			for (; k < i; k++) \
				rb[k] = cmp(BUNtail(bpi, k), nil) != 0; \
		} \
	} while (0)

#define COMPUTE_LEVEL0_COUNT_OTHERS(X, NOTHING1, NOTHING2, NOTHING3) \
	do { \
		computed = cmp(BUNtail(bpi, j + X), nil) != 0; \
	} while (0)
#define ANALYTICAL_COUNT_OTHERS_OTHERS	\
	do { \
		if (count_all) { /* no segment tree required for the global case (it scales in O(n)) */ \
			for (; k < i; k++) \
				rb[k] = (end[k] > start[k]) ? (lng)(end[k] - start[k]) : 0; \
		} else {	\
			oid ncount = i - k; \
			if ((res = rebuild_segmentree(ncount, sizeof(lng), &segment_tree, &tree_capacity, &levels_offset, &levels_capacity, &nlevels)) != GDK_SUCCEED) \
				goto cleanup; \
			populate_segment_tree(lng, ncount, INIT_AGGREGATE_COUNT, COMPUTE_LEVEL0_COUNT_OTHERS, COMPUTE_LEVELN_COUNT, NOTHING, NOTHING, NOTHING); \
			for (; k < i; k++) \
				compute_on_segment_tree(lng, start[k] - j, end[k] - j, INIT_AGGREGATE_COUNT, COMPUTE_LEVELN_COUNT, FINALIZE_AGGREGATE_COUNT, NOTHING, NOTHING, NOTHING); \
			j = k; \
		}	\
	} while (0)

/* Now do the count analytic function branches */
#define ANALYTICAL_COUNT_FIXED_PARTITIONS(TPE, IMP)		\
	do {					\
		TPE *bp = (TPE*) bheap; \
		if (p) {					\
			for (; i < cnt; i++) {		\
				if (np[i]) 			\
					ANALYTICAL_COUNT_FIXED_##IMP(TPE); \
			}						\
		}	\
		i = cnt;			\
		ANALYTICAL_COUNT_FIXED_##IMP(TPE);	\
	} while (0)

#ifdef HAVE_HGE
#define ANALYTICAL_COUNT_LIMIT(IMP)			\
	case TYPE_hge:					\
		ANALYTICAL_COUNT_FIXED_PARTITIONS(hge, IMP);	\
	break;
#else
#define ANALYTICAL_COUNT_LIMIT(IMP)
#endif

#define ANALYTICAL_COUNT_BRANCHES(IMP)		\
	do { \
		switch (ATOMbasetype(tpe)) {		\
		case TYPE_bte:					\
			ANALYTICAL_COUNT_FIXED_PARTITIONS(bte, IMP);		\
			break;							\
		case TYPE_sht:							\
			ANALYTICAL_COUNT_FIXED_PARTITIONS(sht, IMP);		\
			break;							\
		case TYPE_int:							\
			ANALYTICAL_COUNT_FIXED_PARTITIONS(int, IMP);		\
			break;							\
		case TYPE_lng:							\
			ANALYTICAL_COUNT_FIXED_PARTITIONS(lng, IMP);		\
			break;							\
			ANALYTICAL_COUNT_LIMIT(IMP)			\
		case TYPE_flt:							\
			ANALYTICAL_COUNT_FIXED_PARTITIONS(flt, IMP);		\
			break;							\
		case TYPE_dbl:							\
			ANALYTICAL_COUNT_FIXED_PARTITIONS(dbl, IMP);		\
			break;							\
		default: {							\
			if (p) {						\
				for (; i < cnt; i++) {			\
					if (np[i]) 			\
						ANALYTICAL_COUNT_OTHERS_##IMP; \
				}						\
			}	\
			i = cnt;				\
			ANALYTICAL_COUNT_OTHERS_##IMP;	\
		}								\
		}								\
	} while (0)

gdk_return
GDKanalyticalcount(BAT *r, BAT *p, BAT *o, BAT *b, BAT *s, BAT *e, bit ignore_nils, int tpe, int frame_type)
{
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL,
		*levels_offset = NULL, tree_capacity = 0, nlevels = 0, levels_capacity = 0;
	lng curval = 0, *restrict rb = (lng *) Tloc(r, 0);
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL;
	const void *restrict nil = ATOMnilptr(tpe);
	int (*cmp) (const void *, const void *) = ATOMcompare(tpe);
	const void *restrict bheap = Tloc(b, 0);
	bool count_all = !ignore_nils || b->tnonil;
	BATiter bpi = bat_iterator(b);
	void *segment_tree = NULL;
	gdk_return res = GDK_SUCCEED;

	if (cnt > 0) {
		switch (frame_type) {
		case 3: /* unbounded until current row */	{
			ANALYTICAL_COUNT_BRANCHES(UNBOUNDED_TILL_CURRENT_ROW);
		} break;
		case 4: /* current row until unbounded */	{
			ANALYTICAL_COUNT_BRANCHES(CURRENT_ROW_TILL_UNBOUNDED);
		} break;
		case 5: /* all rows */	{
			ANALYTICAL_COUNT_BRANCHES(ALL_ROWS);
		} break;
		case 6: /* current row */ {
			ANALYTICAL_COUNT_BRANCHES(CURRENT_ROW);
		} break;
		default: {
			ANALYTICAL_COUNT_BRANCHES(OTHERS);
		}
		}
	}

	BATsetcount(r, cnt);
	r->tnonil = true;
	r->tnil = false;
cleanup:
	GDKfree(segment_tree);
	GDKfree(levels_offset);
	return res;
}

/* sum on fixed size integers */
#define ANALYTICAL_SUM_IMP_NUM_UNBOUNDED_TILL_CURRENT_ROW(TPE1, TPE2) \
	do { \
		TPE2 curval = TPE2##_nil; \
		for (; k < i;) { \
			j = k; \
			do {	\
				if (!is_##TPE1##_nil(bp[k])) {		\
					if (is_##TPE2##_nil(curval))	\
						curval = (TPE2) bp[k];	\
					else				\
						ADD_WITH_CHECK(bp[k], curval, TPE2, curval, GDK_##TPE2##_max, goto calc_overflow); \
				}					\
				k++; \
			} while (k < i && !op[k]);	\
			for (; j < k; j++) \
				rb[j] = curval; \
			has_nils |= is_##TPE2##_nil(curval);	\
		} \
	} while (0)

#define ANALYTICAL_SUM_IMP_NUM_CURRENT_ROW_TILL_UNBOUNDED(TPE1, TPE2) \
	do { \
		TPE2 curval = TPE2##_nil; \
		l = i - 1; \
		for (j = l; ; j--) { \
			if (!is_##TPE1##_nil(bp[j])) {		\
				if (is_##TPE2##_nil(curval))	\
					curval = (TPE2) bp[j];	\
				else				\
					ADD_WITH_CHECK(bp[j], curval, TPE2, curval, GDK_##TPE2##_max, goto calc_overflow); \
			}					\
			if (op[j] || j == k) {	\
				for (; ; l--) { \
					rb[l] = curval; \
					if (l == j)	\
						break;	\
				} \
				has_nils |= is_##TPE2##_nil(curval);	\
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		k = i; \
	} while (0)

#define ANALYTICAL_SUM_IMP_NUM_ALL_ROWS(TPE1, TPE2)	\
	do { \
		TPE2 curval = TPE2##_nil; \
		for (; j < i; j++) { \
			TPE1 v = bp[j]; \
			if (!is_##TPE1##_nil(v)) {		\
				if (is_##TPE2##_nil(curval))	\
					curval = (TPE2) v;	\
				else				\
					ADD_WITH_CHECK(v, curval, TPE2, curval, GDK_##TPE2##_max, goto calc_overflow); \
			}					\
		} \
		for (; k < i; k++) \
			rb[k] = curval; \
		has_nils |= is_##TPE2##_nil(curval);	\
	} while (0)

#define ANALYTICAL_SUM_IMP_NUM_CURRENT_ROW(TPE1, TPE2)	\
	do { \
		for (; k < i; k++) { \
			TPE1 v = bp[k]; \
			if (is_##TPE1##_nil(v)) {	\
				rb[k] = TPE2##_nil; \
				has_nils = true; \
			} else	{		\
				rb[k] = (TPE2) v; \
			} \
		} \
	} while (0)

#define INIT_AGGREGATE_SUM(NOTHING1, TPE2, NOTHING2) \
	do { \
		computed = TPE2##_nil; \
	} while (0)
#define COMPUTE_LEVEL0_SUM(X, TPE1, TPE2, NOTHING) \
	do { \
		TPE1 v = bp[j + X]; \
		computed = is_##TPE1##_nil(v) ? TPE2##_nil : (TPE2) v; \
	} while (0)
#define COMPUTE_LEVELN_SUM_NUM(VAL, NOTHING1, TPE2, NOTHING2) \
	do { \
		if (!is_##TPE2##_nil(VAL)) {		\
			if (is_##TPE2##_nil(computed))	\
				computed = VAL;	\
			else		\
				ADD_WITH_CHECK(VAL, computed, TPE2, computed, GDK_##TPE2##_max, goto calc_overflow); \
		}	\
	} while (0)
#define FINALIZE_AGGREGATE_SUM(NOTHING1, TPE2, NOTHING2) \
	do { \
		rb[k] = computed;	\
		has_nils |= is_##TPE2##_nil(computed); \
	} while (0)
#define ANALYTICAL_SUM_IMP_NUM_OTHERS(TPE1, TPE2)	\
	do { \
		oid ncount = i - k; \
		if ((res = rebuild_segmentree(ncount, sizeof(TPE2), &segment_tree, &tree_capacity, &levels_offset, &levels_capacity, &nlevels)) != GDK_SUCCEED) \
			goto cleanup; \
		populate_segment_tree(TPE2, ncount, INIT_AGGREGATE_SUM, COMPUTE_LEVEL0_SUM, COMPUTE_LEVELN_SUM_NUM, TPE1, TPE2, NOTHING); \
		for (; k < i; k++) \
			compute_on_segment_tree(TPE2, start[k] - j, end[k] - j, INIT_AGGREGATE_SUM, COMPUTE_LEVELN_SUM_NUM, FINALIZE_AGGREGATE_SUM, TPE1, TPE2, NOTHING); \
		j = k; \
	} while (0)

/* sum on floating-points */
/* TODO go through a version of dofsum which returns the current partials for all the cases */
#define ANALYTICAL_SUM_IMP_FP_UNBOUNDED_TILL_CURRENT_ROW(TPE1, TPE2) ANALYTICAL_SUM_IMP_NUM_UNBOUNDED_TILL_CURRENT_ROW(TPE1, TPE2)
#define ANALYTICAL_SUM_IMP_FP_CURRENT_ROW_TILL_UNBOUNDED(TPE1, TPE2) ANALYTICAL_SUM_IMP_NUM_CURRENT_ROW_TILL_UNBOUNDED(TPE1, TPE2)

#define ANALYTICAL_SUM_IMP_FP_ALL_ROWS(TPE1, TPE2)	\
	do { \
		TPE1 *bs = bp + k;	\
		BUN parcel = i - k;	\
		TPE2 curval = TPE2##_nil; \
		if (dofsum(bs, 0,			\
				&(struct canditer){.tpe = cand_dense, .ncand = parcel,}, \
				parcel, &curval, 1, TYPE_##TPE1, \
				TYPE_##TPE2, NULL, 0, 0, true, \
				false, true) == BUN_NONE) {	\
			goto bailout;			\
		}	\
		for (; k < i; k++) \
			rb[k] = curval; \
		has_nils |= is_##TPE2##_nil(curval);	\
	} while (0)

#define ANALYTICAL_SUM_IMP_FP_CURRENT_ROW(TPE1, TPE2) ANALYTICAL_SUM_IMP_NUM_CURRENT_ROW(TPE1, TPE2)
#define ANALYTICAL_SUM_IMP_FP_OTHERS(TPE1, TPE2) ANALYTICAL_SUM_IMP_NUM_OTHERS(TPE1, TPE2)

#define ANALYTICAL_SUM_CALC(TPE1, TPE2, IMP)		\
	do {						\
		TPE1 *bp = (TPE1*)Tloc(b, 0);	 \
		TPE2 *restrict rb = (TPE2*)Tloc(r, 0); \
		if (p) {					\
			for (; i < cnt; i++) {		\
				if (np[i]) 			\
					IMP(TPE1, TPE2);	\
			}						\
		}	\
		i = cnt;					\
		IMP(TPE1, TPE2);	\
	} while (0)

#ifdef HAVE_HGE
#define ANALYTICAL_SUM_LIMIT(IMP)	\
	case TYPE_hge:{		\
		switch (tp1) {		\
		case TYPE_bte:		\
			ANALYTICAL_SUM_CALC(bte, hge, ANALYTICAL_SUM_IMP_NUM_##IMP);		\
			break;			\
		case TYPE_sht:		\
			ANALYTICAL_SUM_CALC(sht, hge, ANALYTICAL_SUM_IMP_NUM_##IMP);		\
			break;			\
		case TYPE_int:		\
			ANALYTICAL_SUM_CALC(int, hge, ANALYTICAL_SUM_IMP_NUM_##IMP);		\
			break;			\
		case TYPE_lng:		\
			ANALYTICAL_SUM_CALC(lng, hge, ANALYTICAL_SUM_IMP_NUM_##IMP);		\
			break;			\
		case TYPE_hge:		\
			ANALYTICAL_SUM_CALC(hge, hge, ANALYTICAL_SUM_IMP_NUM_##IMP);		\
			break;		\
		default:		\
			goto nosupport;		\
		}			\
		break;		\
	}
#else
#define ANALYTICAL_SUM_LIMIT(IMP)
#endif

#define ANALYTICAL_SUM_BRANCHES(IMP)		\
	do { \
		switch (tp2) {		\
		case TYPE_bte:{		\
			switch (tp1) {		\
			case TYPE_bte:		\
				ANALYTICAL_SUM_CALC(bte, bte, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			default:		\
				goto nosupport;		\
			}		\
			break;		\
		}		\
		case TYPE_sht:{		\
			switch (tp1) {		\
			case TYPE_bte:		\
				ANALYTICAL_SUM_CALC(bte, sht, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			case TYPE_sht:		\
				ANALYTICAL_SUM_CALC(sht, sht, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			default:		\
				goto nosupport;		\
			}		\
			break;		\
		}		\
		case TYPE_int:{		\
			switch (tp1) {		\
			case TYPE_bte:		\
				ANALYTICAL_SUM_CALC(bte, int, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			case TYPE_sht:		\
				ANALYTICAL_SUM_CALC(sht, int, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			case TYPE_int:		\
				ANALYTICAL_SUM_CALC(int, int, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			default:		\
				goto nosupport;		\
			}		\
			break;		\
		}		\
		case TYPE_lng:{		\
			switch (tp1) {		\
			case TYPE_bte:		\
				ANALYTICAL_SUM_CALC(bte, lng, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			case TYPE_sht:		\
				ANALYTICAL_SUM_CALC(sht, lng, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			case TYPE_int:		\
				ANALYTICAL_SUM_CALC(int, lng, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			case TYPE_lng:		\
				ANALYTICAL_SUM_CALC(lng, lng, ANALYTICAL_SUM_IMP_NUM_##IMP);	\
				break;		\
			default:		\
				goto nosupport;		\
			}		\
			break;		\
		}		\
		ANALYTICAL_SUM_LIMIT(IMP)		\
		case TYPE_flt:{		\
			switch (tp1) {		\
			case TYPE_flt:		\
				ANALYTICAL_SUM_CALC(flt, flt, ANALYTICAL_SUM_IMP_FP_##IMP);		\
				break;		\
			default:		\
				goto nosupport;		\
			}		\
			break;		\
		}		\
		case TYPE_dbl:{		\
			switch (tp1) {		\
			case TYPE_flt:		\
				ANALYTICAL_SUM_CALC(flt, dbl, ANALYTICAL_SUM_IMP_FP_##IMP);		\
				break;		\
			case TYPE_dbl:		\
				ANALYTICAL_SUM_CALC(dbl, dbl, ANALYTICAL_SUM_IMP_FP_##IMP);		\
				break;		\
			default:		\
				goto nosupport;		\
			}		\
			break;		\
		}		\
		default:		\
			goto nosupport;		\
		}		\
	} while (0)

static gdk_return /* This is a workaround for a MSVC compiler bug at build engine version 16.7.0+b89cb5fde Will test again after the next release */
GDKanalyticalsumothers(BAT *r, BAT *p, bit *np, BAT *b, oid *restrict start, oid *restrict end, int tp1, int tp2)
{
	bool has_nils = false;
	oid i = 0, j = 0, k = 0, cnt = BATcount(b), *levels_offset = NULL, tree_capacity = 0, nlevels = 0, levels_capacity = 0;
	int abort_on_error = 1;
	BUN nils = 0;
	void *segment_tree = NULL;
	gdk_return res = GDK_SUCCEED;

	ANALYTICAL_SUM_BRANCHES(OTHERS);

	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	goto cleanup; /* all these gotos seem confusing but it cleans up the ending of the operator */
calc_overflow:
	GDKerror("22003!overflow in calculation.\n");
	res = GDK_FAIL;
cleanup:
	GDKfree(segment_tree);
	GDKfree(levels_offset);
	return res;
nosupport:
	GDKerror("42000!type combination (sum(%s)->%s) not supported.\n", ATOMname(tp1), ATOMname(tp2));
	return GDK_FAIL;
}

gdk_return
GDKanalyticalsum(BAT *r, BAT *p, BAT *o, BAT *b, BAT *s, BAT *e, int tp1, int tp2, int frame_type)
{
	bool has_nils = false;
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL;
	int abort_on_error = 1;
	BUN nils = 0;

	if (cnt > 0) {
		switch (frame_type) {
		case 3: /* unbounded until current row */	{
			ANALYTICAL_SUM_BRANCHES(UNBOUNDED_TILL_CURRENT_ROW);
		} break;
		case 4: /* current row until unbounded */	{
			ANALYTICAL_SUM_BRANCHES(CURRENT_ROW_TILL_UNBOUNDED);
		} break;
		case 5: /* all rows */	{
			ANALYTICAL_SUM_BRANCHES(ALL_ROWS);
		} break;
		case 6: /* current row */ {
			ANALYTICAL_SUM_BRANCHES(CURRENT_ROW);
		} break;
		default: {
			return GDKanalyticalsumothers(r, p, np, b, start, end, tp1, tp2);
		}
		}
	}

	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
      bailout:
	GDKerror("42000!error while calculating floating-point sum\n");
	return GDK_FAIL;
      nosupport:
	GDKerror("42000!type combination (sum(%s)->%s) not supported.\n", ATOMname(tp1), ATOMname(tp2));
	return GDK_FAIL;
      calc_overflow:
	GDKerror("22003!overflow in calculation.\n");
	return GDK_FAIL;
}

/* product on integers */
#define PROD_NUM(TPE1, TPE2, TPE3, ARG) \
	do {	\
		if (!is_##TPE1##_nil(ARG)) {		\
			if (is_##TPE2##_nil(curval))	\
				curval = (TPE2) ARG;	\
			else				\
				MUL4_WITH_CHECK(ARG, curval, TPE2, curval, GDK_##TPE2##_max, TPE3, goto calc_overflow); \
		}				\
	} while(0)

#define ANALYTICAL_PROD_CALC_NUM_UNBOUNDED_TILL_CURRENT_ROW(TPE1, TPE2, TPE3) \
	do { \
		TPE2 curval = TPE2##_nil; \
		for (; k < i;) { \
			j = k; \
			do {	\
				PROD_NUM(TPE1, TPE2, TPE3, bp[k]); \
				k++; \
			} while (k < i && !op[k]);	\
			for (; j < k; j++) \
				rb[j] = curval; \
			has_nils |= is_##TPE2##_nil(curval);	\
		} \
	} while (0)

#define ANALYTICAL_PROD_CALC_NUM_CURRENT_ROW_TILL_UNBOUNDED(TPE1, TPE2, TPE3) \
	do { \
		TPE2 curval = TPE2##_nil; \
		l = i - 1; \
		for (j = l; ; j--) { \
			PROD_NUM(TPE1, TPE2, TPE3, bp[j]); \
			if (op[j] || j == k) {	\
				for (; ; l--) { \
					rb[l] = curval; \
					if (l == j)	\
						break;	\
				} \
				has_nils |= is_##TPE2##_nil(curval);	\
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		k = i; \
	} while (0)

#define ANALYTICAL_PROD_CALC_NUM_ALL_ROWS(TPE1, TPE2, TPE3)	\
	do { \
		TPE2 curval = TPE2##_nil; \
		for (; j < i; j++) { \
			TPE1 v = bp[j]; \
			PROD_NUM(TPE1, TPE2, TPE3, v); \
		} \
		for (; k < i; k++) \
			rb[k] = curval; \
		has_nils |= is_##TPE2##_nil(curval);	\
	} while (0)

#define ANALYTICAL_PROD_CALC_NUM_CURRENT_ROW(TPE1, TPE2, TPE3)	\
	do {								\
		for (; k < i; k++) { \
			TPE1 v = bp[k]; \
			if (is_##TPE1##_nil(v)) {	\
				rb[k] = TPE2##_nil; \
				has_nils = true; \
			} else	{		\
				rb[k] = (TPE2) v; \
			} \
		} \
	} while (0)

#define INIT_AGGREGATE_PROD(NOTHING1, TPE2, NOTHING2) \
	do { \
		computed = TPE2##_nil; \
	} while (0)
#define COMPUTE_LEVEL0_PROD(X, TPE1, TPE2, NOTHING) \
	do { \
		TPE1 v = bp[j + X]; \
		computed = is_##TPE1##_nil(v) ? TPE2##_nil : (TPE2) v; \
	} while (0)
#define COMPUTE_LEVELN_PROD_NUM(VAL, NOTHING, TPE2, TPE3) \
	do { \
		if (!is_##TPE2##_nil(VAL)) {		\
			if (is_##TPE2##_nil(computed))	\
				computed = VAL;	\
			else				\
				MUL4_WITH_CHECK(VAL, computed, TPE2, computed, GDK_##TPE2##_max, TPE3, goto calc_overflow); \
		}				\
	} while (0)
#define FINALIZE_AGGREGATE_PROD(NOTHING1, TPE2, NOTHING2) \
	do { \
		rb[k] = computed;	\
		has_nils |= is_##TPE2##_nil(computed); \
	} while (0)
#define ANALYTICAL_PROD_CALC_NUM_OTHERS(TPE1, TPE2, TPE3)	\
	do { \
		oid ncount = i - k; \
		if ((res = rebuild_segmentree(ncount, sizeof(TPE2), &segment_tree, &tree_capacity, &levels_offset, &levels_capacity, &nlevels)) != GDK_SUCCEED) \
			goto cleanup; \
		populate_segment_tree(TPE2, ncount, INIT_AGGREGATE_PROD, COMPUTE_LEVEL0_PROD, COMPUTE_LEVELN_PROD_NUM, TPE1, TPE2, TPE3); \
		for (; k < i; k++) \
			compute_on_segment_tree(TPE2, start[k] - j, end[k] - j, INIT_AGGREGATE_PROD, COMPUTE_LEVELN_PROD_NUM, FINALIZE_AGGREGATE_PROD, TPE1, TPE2, TPE3); \
		j = k; \
	} while (0)

/* product on integers while checking for overflows on the output  */
#define PROD_NUM_LIMIT(TPE1, TPE2, REAL_IMP, ARG) \
	do {	\
		if (!is_##TPE1##_nil(ARG)) {		\
			if (is_##TPE2##_nil(curval))	\
				curval = (TPE2) ARG;	\
			else				\
				REAL_IMP(ARG, curval, curval, GDK_##TPE2##_max, goto calc_overflow); \
		}					\
	} while(0)

#define ANALYTICAL_PROD_CALC_NUM_LIMIT_UNBOUNDED_TILL_CURRENT_ROW(TPE1, TPE2, REAL_IMP) \
	do { \
		TPE2 curval = TPE2##_nil; \
		for (; k < i;) { \
			j = k; \
			do {	\
				PROD_NUM_LIMIT(TPE1, TPE2, REAL_IMP, bp[k]); \
				k++; \
			} while (k < i && !op[k]);	\
			for (; j < k; j++) \
				rb[j] = curval; \
			has_nils |= is_##TPE2##_nil(curval);	\
		} \
	} while (0)

#define ANALYTICAL_PROD_CALC_NUM_LIMIT_CURRENT_ROW_TILL_UNBOUNDED(TPE1, TPE2, REAL_IMP) \
	do { \
		TPE2 curval = TPE2##_nil; \
		l = i - 1; \
		for (j = l; ; j--) { \
			PROD_NUM_LIMIT(TPE1, TPE2, REAL_IMP, bp[j]); \
			if (op[j] || j == k) {	\
				for (; ; l--) { \
					rb[l] = curval; \
					if (l == j)	\
						break;	\
				} \
				has_nils |= is_##TPE2##_nil(curval);	\
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		k = i; \
	} while (0)

#define ANALYTICAL_PROD_CALC_NUM_LIMIT_ALL_ROWS(TPE1, TPE2, REAL_IMP)	\
	do { \
		TPE2 curval = TPE2##_nil; \
		for (; j < i; j++) { \
			TPE1 v = bp[j]; \
			PROD_NUM_LIMIT(TPE1, TPE2, REAL_IMP, v); \
		} \
		for (; k < i; k++) \
			rb[k] = curval; \
		has_nils |= is_##TPE2##_nil(curval);	\
	} while (0)

#define ANALYTICAL_PROD_CALC_NUM_LIMIT_CURRENT_ROW(TPE1, TPE2, REAL_IMP)	\
	do {								\
		for (; k < i; k++) { \
			TPE1 v = bp[k]; \
			if (is_##TPE1##_nil(v)) {	\
				rb[k] = TPE2##_nil; \
				has_nils = true; \
			} else	{		\
				rb[k] = (TPE2) v; \
			} \
		} \
	} while (0)

#define COMPUTE_LEVELN_PROD_NUM_LIMIT(VAL, NOTHING, TPE2, REAL_IMP) \
	do { \
		if (!is_##TPE2##_nil(VAL)) {		\
			if (is_##TPE2##_nil(computed))	\
				computed = VAL;	\
			else				\
				REAL_IMP(VAL, computed, computed, GDK_##TPE2##_max, goto calc_overflow); \
		}				\
	} while (0)
#define ANALYTICAL_PROD_CALC_NUM_LIMIT_OTHERS(TPE1, TPE2, REAL_IMP)	\
	do { \
		oid ncount = i - k; \
		if ((res = rebuild_segmentree(ncount, sizeof(TPE2), &segment_tree, &tree_capacity, &levels_offset, &levels_capacity, &nlevels)) != GDK_SUCCEED) \
			goto cleanup; \
		populate_segment_tree(TPE2, ncount, INIT_AGGREGATE_PROD, COMPUTE_LEVEL0_PROD, COMPUTE_LEVELN_PROD_NUM_LIMIT, TPE1, TPE2, REAL_IMP); \
		for (; k < i; k++) \
			compute_on_segment_tree(TPE2, start[k] - j, end[k] - j, INIT_AGGREGATE_PROD, COMPUTE_LEVELN_PROD_NUM_LIMIT, FINALIZE_AGGREGATE_PROD, TPE1, TPE2, REAL_IMP); \
		j = k; \
	} while (0)

/* product on floating-points */
#define PROD_FP(TPE1, TPE2, ARG) \
	do {	\
		if (!is_##TPE1##_nil(ARG)) {		\
			if (is_##TPE2##_nil(curval)) {	\
				curval = (TPE2) ARG;	\
			} else if (ABSOLUTE(curval) > 1 && GDK_##TPE2##_max / ABSOLUTE(ARG) < ABSOLUTE(curval)) { \
				if (abort_on_error)	\
					goto calc_overflow; \
				curval = TPE2##_nil;	\
				nils++;			\
			} else {			\
				curval *= ARG;		\
			}			\
		}			\
	} while(0)

#define ANALYTICAL_PROD_CALC_FP_UNBOUNDED_TILL_CURRENT_ROW(TPE1, TPE2, ARG3)	/* ARG3 is ignored here */ \
	do { \
		TPE2 curval = TPE2##_nil; \
		for (; k < i;) { \
			j = k; \
			do {	\
				PROD_FP(TPE1, TPE2, bp[k]);	\
				k++; \
			} while (k < i && !op[k]);	\
			for (; j < k; j++) \
				rb[j] = curval; \
			has_nils |= is_##TPE2##_nil(curval);	\
		} \
	} while (0)

#define ANALYTICAL_PROD_CALC_FP_CURRENT_ROW_TILL_UNBOUNDED(TPE1, TPE2, ARG3)	/* ARG3 is ignored here */ \
	do { \
		TPE2 curval = TPE2##_nil; \
		l = i - 1; \
		for (j = l; ; j--) { \
			PROD_FP(TPE1, TPE2, bp[j]);	\
			if (op[j] || j == k) {	\
				for (; ; l--) { \
					rb[l] = curval; \
					if (l == j)	\
						break;	\
				} \
				has_nils |= is_##TPE2##_nil(curval);	\
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		k = i; \
	} while (0)

#define ANALYTICAL_PROD_CALC_FP_ALL_ROWS(TPE1, TPE2, ARG3)	/* ARG3 is ignored here */	\
	do { \
		TPE2 curval = TPE2##_nil; \
		for (; j < i; j++) { \
			TPE1 v = bp[j]; \
			PROD_FP(TPE1, TPE2, v); \
		} \
		for (; k < i; k++) \
			rb[k] = curval; \
		has_nils |= is_##TPE2##_nil(curval);	\
	} while (0)

#define ANALYTICAL_PROD_CALC_FP_CURRENT_ROW(TPE1, TPE2, ARG3)	/* ARG3 is ignored here */	\
	do {								\
		for (; k < i; k++) { \
			TPE1 v = bp[k]; \
			if (is_##TPE1##_nil(v)) {	\
				rb[k] = TPE2##_nil; \
				has_nils = true; \
			} else	{		\
				rb[k] = (TPE2) v; \
			} \
		} \
	} while (0)

#define COMPUTE_LEVELN_PROD_FP(VAL, NOTHING1, TPE2, NOTHING2) \
	do { \
		if (!is_##TPE2##_nil(VAL)) {		\
			if (is_##TPE2##_nil(computed)) {	\
				computed = VAL;	\
			} else if (ABSOLUTE(computed) > 1 && GDK_##TPE2##_max / ABSOLUTE(VAL) < ABSOLUTE(computed)) { \
				if (abort_on_error)	\
					goto calc_overflow; \
				computed = TPE2##_nil;	\
				nils++;			\
			} else {			\
				computed *= VAL;		\
			}			\
		}			\
	} while (0)
#define ANALYTICAL_PROD_CALC_FP_OTHERS(TPE1, TPE2, ARG3) /* ARG3 is ignored here */ \
	do { \
		oid ncount = i - k; \
		if ((res = rebuild_segmentree(ncount, sizeof(TPE2), &segment_tree, &tree_capacity, &levels_offset, &levels_capacity, &nlevels)) != GDK_SUCCEED) \
			goto cleanup; \
		populate_segment_tree(TPE2, ncount, INIT_AGGREGATE_PROD, COMPUTE_LEVEL0_PROD, COMPUTE_LEVELN_PROD_FP, TPE1, TPE2, ARG3); \
		for (; k < i; k++) \
			compute_on_segment_tree(TPE2, start[k] - j, end[k] - j, INIT_AGGREGATE_PROD, COMPUTE_LEVELN_PROD_FP, FINALIZE_AGGREGATE_PROD, TPE1, TPE2, ARG3); \
		j = k; \
	} while (0)

#define ANALYTICAL_PROD_CALC_NUM_PARTITIONS(TPE1, TPE2, TPE3_OR_REAL_IMP, IMP)		\
	do {						\
		TPE1 *bp = (TPE1*)Tloc(b, 0);	 \
		TPE2 *restrict rb = (TPE2*)Tloc(r, 0); \
		if (p) {					\
			for (; i < cnt; i++) {		\
				if (np[i]) 			\
					IMP(TPE1, TPE2, TPE3_OR_REAL_IMP);	\
			}						\
		}	\
		i = cnt;					\
		IMP(TPE1, TPE2, TPE3_OR_REAL_IMP);	\
	} while (0)

#ifdef HAVE_HGE
#define ANALYTICAL_PROD_LIMIT(IMP)	\
	case TYPE_lng:{	\
		switch (tp1) {	\
		case TYPE_bte:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(bte, lng, hge, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
			break;	\
		case TYPE_sht:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(sht, lng, hge, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
			break;	\
		case TYPE_int:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(int, lng, hge, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
			break;	\
		case TYPE_lng:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(lng, lng, hge, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
			break;	\
		default:	\
			goto nosupport;	\
		}	\
		break;	\
	}	\
	case TYPE_hge:{	\
		switch (tp1) {	\
		case TYPE_bte:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(bte, hge, HGEMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		case TYPE_sht:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(sht, hge, HGEMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		case TYPE_int:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(int, hge, HGEMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		case TYPE_lng:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(lng, hge, HGEMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		case TYPE_hge:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(hge, hge, HGEMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		default:	\
			goto nosupport;	\
		}	\
		break;	\
	}
#else
#define ANALYTICAL_PROD_LIMIT(IMP)	\
	case TYPE_lng:{	\
		switch (tp1) {	\
		case TYPE_bte:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(bte, lng, LNGMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		case TYPE_sht:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(sht, lng, LNGMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		case TYPE_int:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(int, lng, LNGMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		case TYPE_lng:	\
			ANALYTICAL_PROD_CALC_NUM_PARTITIONS(lng, lng, LNGMUL_CHECK, ANALYTICAL_PROD_CALC_NUM_LIMIT_##IMP);	\
			break;	\
		default:	\
			goto nosupport;	\
		}	\
		break;	\
	}
#endif

#define ANALYTICAL_PROD_BRANCHES(IMP)		\
	do { \
		switch (tp2) {	\
		case TYPE_bte:{	\
			switch (tp1) {	\
			case TYPE_bte:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(bte, bte, sht, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
				break;	\
			default:	\
				goto nosupport;	\
			}	\
			break;	\
		}	\
		case TYPE_sht:{	\
			switch (tp1) {	\
			case TYPE_bte:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(bte, sht, int, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
				break;	\
			case TYPE_sht:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(sht, sht, int, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
				break;	\
			default:	\
				goto nosupport;	\
			}	\
			break;	\
		}	\
		case TYPE_int:{	\
			switch (tp1) {	\
			case TYPE_bte:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(bte, int, lng, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
				break;	\
			case TYPE_sht:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(sht, int, lng, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
				break;	\
			case TYPE_int:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(int, int, lng, ANALYTICAL_PROD_CALC_NUM_##IMP);	\
				break;	\
			default:	\
				goto nosupport;	\
			}	\
			break;	\
		}	\
		ANALYTICAL_PROD_LIMIT(IMP)	\
		case TYPE_flt:{	\
			switch (tp1) {	\
			case TYPE_flt:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(flt, flt, ;, ANALYTICAL_PROD_CALC_FP_##IMP);	\
				break;	\
			default:	\
				goto nosupport;	\
			}	\
			break;	\
		}	\
		case TYPE_dbl:{	\
			switch (tp1) {	\
			case TYPE_flt:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(flt, dbl, ;, ANALYTICAL_PROD_CALC_FP_##IMP);	\
				break;	\
			case TYPE_dbl:	\
				ANALYTICAL_PROD_CALC_NUM_PARTITIONS(dbl, dbl, ;, ANALYTICAL_PROD_CALC_FP_##IMP);	\
				break;	\
			default:	\
				goto nosupport;	\
			}	\
			break;	\
		}	\
		default:	\
			goto nosupport;	\
		}	\
	} while (0)

gdk_return
GDKanalyticalprod(BAT *r, BAT *p, BAT *o, BAT *b, BAT *s, BAT *e, int tp1, int tp2, int frame_type)
{
	bool has_nils = false;
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL,
		*levels_offset = NULL, tree_capacity = 0, nlevels = 0, levels_capacity = 0;
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL;
	int abort_on_error = 1;
	BUN nils = 0;
	void *segment_tree = NULL;
	gdk_return res = GDK_SUCCEED;

	if (cnt > 0) {
		switch (frame_type) {
		case 3: /* unbounded until current row */	{
			ANALYTICAL_PROD_BRANCHES(UNBOUNDED_TILL_CURRENT_ROW);
		} break;
		case 4: /* current row until unbounded */	{
			ANALYTICAL_PROD_BRANCHES(CURRENT_ROW_TILL_UNBOUNDED);
		} break;
		case 5: /* all rows */	{
			ANALYTICAL_PROD_BRANCHES(ALL_ROWS);
		} break;
		case 6: /* current row */ {
			ANALYTICAL_PROD_BRANCHES(CURRENT_ROW);
		} break;
		default: {
			ANALYTICAL_PROD_BRANCHES(OTHERS);
		}
		}
	}

	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	goto cleanup; /* all these gotos seem confusing but it cleans up the ending of the operator */
calc_overflow:
	GDKerror("22003!overflow in calculation.\n");
	res = GDK_FAIL;
cleanup:
	GDKfree(segment_tree);
	GDKfree(levels_offset);
	return res;
nosupport:
	GDKerror("42000!type combination (prod(%s)->%s) not supported.\n", ATOMname(tp1), ATOMname(tp2));
	return GDK_FAIL;
}

#ifdef HAVE_HGE
#define LNG_HGE hge
#else
#define LNG_HGE lng
#endif

/* average on integers */
#define ANALYTICAL_AVERAGE_CALC_NUM_STEP1(TPE, IMP, PART, LNG_HGE, ARG) \
	if (!is_##TPE##_nil(ARG)) {		\
		ADD_WITH_CHECK(ARG, sum, LNG_HGE, sum, GDK_##LNG_HGE##_max, goto avg_overflow##TPE##IMP##PART); \
		/* count only when no overflow occurs */ \
		n++;				\
	}

#define ANALYTICAL_AVERAGE_CALC_NUM_STEP2(TPE, IMP, PART) \
			if (0) {					\
avg_overflow##TPE##IMP##PART:							\
				assert(n > 0);				\
				if (sum >= 0) {				\
					a = (TPE) (sum / n);		\
					rr = (lng) (sum % n);		\
				} else {				\
					sum = -sum;			\
					a = - (TPE) (sum / n);		\
					rr = (lng) (sum % n);		\
					if (r) {			\
						a--;			\
						rr = n - rr;		\
					}				\
				}

#define ANALYTICAL_AVG_IMP_NUM_UNBOUNDED_TILL_CURRENT_ROW(TPE, IMP, PART, LNG_HGE) \
	do { \
		TPE a = 0; \
		dbl curval = dbl_nil;	\
		for (; k < i;) { \
			j = k; \
			do {	\
				ANALYTICAL_AVERAGE_CALC_NUM_STEP1(TPE, IMP, PART, LNG_HGE, bp[k]) \
				ANALYTICAL_AVERAGE_CALC_NUM_STEP2(TPE, IMP, PART) \
					while (k < i && !op[k]) { \
						TPE v = bp[k++];			\
						if (is_##TPE##_nil(v))		\
							continue;		\
						AVERAGE_ITER(TPE, v, a, rr, n);	\
					}					\
					curval = a + (dbl) rr / n;		\
					goto calc_done##TPE##IMP##PART;			\
				}						\
				k++; \
			} while (k < i && !op[k]);	\
			curval = n > 0 ? (dbl) sum / n : dbl_nil;	\
calc_done##TPE##IMP##PART: \
			for (; j < k; j++) \
				rb[j] = curval; \
			has_nils |= (n == 0);		\
		} \
		n = 0;				\
		sum = 0;			\
	} while (0)

#define ANALYTICAL_AVG_IMP_NUM_CURRENT_ROW_TILL_UNBOUNDED(TPE, IMP, PART, LNG_HGE) \
	do { \
		TPE a = 0; \
		dbl curval = dbl_nil;	\
		l = i - 1; \
		for (j = l; ; j--) { \
			ANALYTICAL_AVERAGE_CALC_NUM_STEP1(TPE, IMP, PART, LNG_HGE, bp[j]) \
			ANALYTICAL_AVERAGE_CALC_NUM_STEP2(TPE, IMP, PART) \
				while (!(op[j] || j == k)) { \
					TPE v = bp[j--];			\
					if (is_##TPE##_nil(v))		\
						continue;		\
					AVERAGE_ITER(TPE, v, a, rr, n);	\
				}					\
				curval = a + (dbl) rr / n;		\
				goto calc_done##TPE##IMP##PART;			\
			}	\
			if (op[j] || j == k) {	\
				curval = n > 0 ? (dbl) sum / n : dbl_nil;	\
calc_done##TPE##IMP##PART: \
				for (; ; l--) { \
					rb[l] = curval; \
					if (l == j)	\
						break;	\
				} \
				has_nils |= (n == 0);		\
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		n = 0;				\
		sum = 0;			\
		k = i; \
	} while (0)

#define ANALYTICAL_AVG_IMP_NUM_ALL_ROWS(TPE, IMP, PART, LNG_HGE)	\
	do { \
		TPE a = 0; \
		for (; j < i; j++) { \
			TPE v = bp[j]; \
			ANALYTICAL_AVERAGE_CALC_NUM_STEP1(TPE, IMP, PART, LNG_HGE, v) \
			ANALYTICAL_AVERAGE_CALC_NUM_STEP2(TPE, IMP, PART) \
				for (; j < i; j++) { \
					v = bp[j];			\
					if (is_##TPE##_nil(v))		\
						continue;		\
					AVERAGE_ITER(TPE, v, a, rr, n);	\
				}					\
				curval = a + (dbl) rr / n;		\
				goto calc_done##TPE##IMP##PART;			\
			}	\
		} \
		curval = n > 0 ? (dbl) sum / n : dbl_nil;	\
calc_done##TPE##IMP##PART: \
		for (; k < i; k++) \
			rb[k] = curval; \
		has_nils |= (n == 0);		\
		n = 0;				\
		sum = 0;			\
	} while (0)

#define ANALYTICAL_AVG_IMP_NUM_CURRENT_ROW(TPE, IMP, PART, LNG_HGE)	\
	do { \
		for (; k < i; k++) { \
			TPE v = bp[k]; \
			if (is_##TPE##_nil(v)) {	\
				rb[k] = dbl_nil; \
				has_nils = true; \
			} else	{		\
				rb[k] = (dbl) v; \
			} \
		} \
	} while (0)

#define ANALYTICAL_AVG_IMP_NUM_OTHERS(TPE, IMP, PART, LNG_HGE)	\
	do {								\
		TPE *be = 0, *bs = 0, a = 0;	 \
		for (; k < i; k++) {				\
			bs = bp + start[k];				\
			be = bp + end[k];				\
			for (; bs < be; bs++) {				\
				TPE v = *bs;				\
				ANALYTICAL_AVERAGE_CALC_NUM_STEP1(TPE, IMP, PART, LNG_HGE, v) \
			}						\
			ANALYTICAL_AVERAGE_CALC_NUM_STEP2(TPE, IMP, PART) \
				for (; bs < be; bs++) {			\
					TPE v = *bs;			\
					if (is_##TPE##_nil(v))		\
						continue;		\
					AVERAGE_ITER(TPE, v, a, rr, n);	\
				}					\
				curval = a + (dbl) rr / n;		\
				goto calc_done##TPE##IMP##PART;			\
			}	\
			curval = n > 0 ? (dbl) sum / n : dbl_nil;	\
calc_done##TPE##IMP##PART: \
			rb[k] = curval;					\
			has_nils |= (n == 0);		\
			n = 0;				\
			sum = 0;			\
		}							\
	} while (0)

/* average on floating-points */
#define ANALYTICAL_AVG_IMP_FP_UNBOUNDED_TILL_CURRENT_ROW(TPE, IMP, PART, LNG_HGE) \
	do { \
		TPE a = 0; \
		dbl curval = dbl_nil;	\
		for (; k < i;) { \
			j = k; \
			do {	\
				if (!is_##TPE##_nil(bp[k]))		\
					AVERAGE_ITER_FLOAT(TPE, bp[k], a, n); \
				k++; \
			} while (k < i && !op[k]);	\
			if (n > 0)	\
				curval = a;	\
			else	\
				has_nils = true;	\
			for (; j < k; j++) \
				rb[j] = curval; \
		} \
		n = 0;			\
	} while (0)

#define ANALYTICAL_AVG_IMP_FP_CURRENT_ROW_TILL_UNBOUNDED(TPE, IMP, PART, LNG_HGE) \
	do { \
		TPE a = 0; \
		dbl curval = dbl_nil;	\
		l = i - 1; \
		for (j = l; ; j--) { \
			if (!is_##TPE##_nil(bp[j]))		\
				AVERAGE_ITER_FLOAT(TPE, bp[j], a, n); \
			if (op[j] || j == k) {	\
				for (; ; l--) { \
					rb[l] = curval; \
					if (l == j)	\
						break;	\
				} \
				has_nils |= is_##TPE##_nil(curval);	\
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		n = 0;		\
		k = i; \
	} while (0)

#define ANALYTICAL_AVG_IMP_FP_ALL_ROWS(TPE, IMP, PART, LNG_HGE)	\
	do { \
		TPE a = 0; \
		dbl curval = dbl_nil;	\
		for (; j < i; j++) { \
			TPE v = bp[j]; \
			if (!is_##TPE##_nil(v))		\
				AVERAGE_ITER_FLOAT(TPE, v, a, n); \
		} \
		if (n > 0)	\
			curval = a;	\
		else	\
			has_nils = true;	\
		for (; k < i; k++) \
			rb[k] = curval; \
		n = 0;		\
	} while (0)

#define ANALYTICAL_AVG_IMP_FP_CURRENT_ROW(TPE, IMP, PART, LNG_HGE)	 ANALYTICAL_AVG_IMP_NUM_CURRENT_ROW(TPE, IMP, PART, LNG_HGE)

#define ANALYTICAL_AVG_IMP_FP_OTHERS(TPE, IMP, PART, LNG_HGE)			\
	do {								\
		TPE a = 0; \
		for (; k < i; k++) {				\
			TPE *bs = bp + start[k];				\
			TPE *be = bp + end[k];				\
			for (; bs < be; bs++) {				\
				TPE v = *bs;				\
				if (!is_##TPE##_nil(v))			\
					AVERAGE_ITER_FLOAT(TPE, v, a, n); \
			}						\
			curval = (n > 0) ? a : dbl_nil;			\
			rb[k] = curval;					\
			has_nils |= (n == 0);		\
			n = 0;			\
		}							\
	} while (0)

#define ANALYTICAL_AVG_PARTITIONS(TPE, IMP, LNG_HGE, REAL_IMP)		\
	do {						\
		TPE *bp = (TPE*)Tloc(b, 0); \
		if (p) {					\
			for (; i < cnt; i++) {		\
				if (np[i]) 			\
					REAL_IMP(TPE, IMP, P1, LNG_HGE);	\
			}						\
		}			\
		i = cnt;					\
		REAL_IMP(TPE, IMP, P3, LNG_HGE);	\
	} while (0)

#ifdef HAVE_HGE
#define ANALYTICAL_AVG_LIMIT(IMP, LNG_HGE) \
	case TYPE_hge: \
		ANALYTICAL_AVG_PARTITIONS(hge, IMP, LNG_HGE, ANALYTICAL_AVG_IMP_NUM_##IMP); \
		break;
#else
#define ANALYTICAL_AVG_LIMIT(IMP, LNG_HGE)
#endif

#define ANALYTICAL_AVG_BRANCHES(IMP, LNG_HGE)		\
	do { \
		switch (tpe) {	\
		case TYPE_bte:	\
			ANALYTICAL_AVG_PARTITIONS(bte, IMP, LNG_HGE, ANALYTICAL_AVG_IMP_NUM_##IMP);	\
			break;	\
		case TYPE_sht:	\
			ANALYTICAL_AVG_PARTITIONS(sht, IMP, LNG_HGE, ANALYTICAL_AVG_IMP_NUM_##IMP);	\
			break;	\
		case TYPE_int:	\
			ANALYTICAL_AVG_PARTITIONS(int, IMP, LNG_HGE, ANALYTICAL_AVG_IMP_NUM_##IMP);	\
			break;	\
		case TYPE_lng:	\
			ANALYTICAL_AVG_PARTITIONS(lng, IMP, LNG_HGE, ANALYTICAL_AVG_IMP_NUM_##IMP);	\
			break;	\
		ANALYTICAL_AVG_LIMIT(IMP, LNG_HGE)	\
		case TYPE_flt:	\
			ANALYTICAL_AVG_PARTITIONS(flt, IMP, LNG_HGE, ANALYTICAL_AVG_IMP_FP_##IMP);	\
			break;	\
		case TYPE_dbl:	\
			ANALYTICAL_AVG_PARTITIONS(dbl, IMP, LNG_HGE, ANALYTICAL_AVG_IMP_FP_##IMP);	\
			break;	\
		default:	\
			goto nosupport;	\
		}	\
	} while (0)

gdk_return
GDKanalyticalavg(BAT *r, BAT *p, BAT *o, BAT *b, BAT *s, BAT *e, int tpe, int frame_type)
{
	bool has_nils = false;
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;
	lng n = 0, rr = 0;
	dbl *restrict rb = (dbl *) Tloc(r, 0), curval = dbl_nil;
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL;
	bool abort_on_error = true;
	BUN nils = 0;
#ifdef HAVE_HGE
	hge sum = 0;
#else
	lng sum = 0;
#endif

	if (cnt > 0) {
		switch (frame_type) {
		case 3: /* unbounded until current row */	{
			ANALYTICAL_AVG_BRANCHES(UNBOUNDED_TILL_CURRENT_ROW, LNG_HGE);
		} break;
		case 4: /* current row until unbounded */	{
			ANALYTICAL_AVG_BRANCHES(CURRENT_ROW_TILL_UNBOUNDED, LNG_HGE);
		} break;
		case 5: /* all rows */	{
			ANALYTICAL_AVG_BRANCHES(ALL_ROWS, LNG_HGE);
		} break;
		case 6: /* current row */ {
			ANALYTICAL_AVG_BRANCHES(CURRENT_ROW, LNG_HGE);
		} break;
		default: {
			ANALYTICAL_AVG_BRANCHES(OTHERS, LNG_HGE);
		}
		}
	}

	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
      nosupport:
	GDKerror("42000!average of type %s to dbl unsupported.\n", ATOMname(tpe));
	return GDK_FAIL;
}

#ifdef TRUNCATE_NUMBERS
#define ANALYTICAL_AVERAGE_INT_CALC_FINALIZE(avg, rem, ncnt) \
	do {
		if (rem > 0 && avg < 0) \
			avg++; \
	} while(0)
#else
#define ANALYTICAL_AVERAGE_INT_CALC_FINALIZE(avg, rem, ncnt) \
	do { \
		if (rem > 0) { \
			if (avg < 0) { \
				if (2*rem > ncnt) \
					avg++; \
			} else { \
				if (2*rem >= ncnt) \
					avg++; \
			} \
		} \
	} while(0)
#endif

#define ANALYTICAL_AVG_INT_UNBOUNDED_TILL_CURRENT_ROW(TPE) \
	do { \
		TPE avg = 0; \
		for (; k < i;) { \
			j = k; \
			do {	\
				if (!is_##TPE##_nil(bp[k]))	\
					AVERAGE_ITER(TPE, bp[k], avg, rem, ncnt); \
				k++; \
			} while (k < i && !op[k]);	\
			if (ncnt == 0) {	\
				has_nils = true; \
				for (; j < k; j++) \
					rb[j] = TPE##_nil; \
			} else { \
				TPE avgfinal = avg; \
				ANALYTICAL_AVERAGE_INT_CALC_FINALIZE(avgfinal, rem, ncnt); \
				for (; j < k; j++) \
					rb[j] = avgfinal; \
			} \
		} \
		rem = 0; \
		ncnt = 0; \
	} while (0)

#define ANALYTICAL_AVG_INT_CURRENT_ROW_TILL_UNBOUNDED(TPE) \
	do { \
		TPE avg = 0; \
		l = i - 1; \
		for (j = l; ; j--) { \
			if (!is_##TPE##_nil(bp[j]))	\
				AVERAGE_ITER(TPE, bp[j], avg, rem, ncnt); \
			if (op[j] || j == k) {	\
				if (ncnt == 0) {	\
					has_nils = true; \
					for (; ; l--) { \
						rb[l] = TPE##_nil; \
						if (l == j)	\
							break;	\
					} \
				} else { \
					TPE avgfinal = avg; \
					ANALYTICAL_AVERAGE_INT_CALC_FINALIZE(avgfinal, rem, ncnt); \
					for (; ; l--) { \
						rb[l] = avgfinal; \
						if (l == j)	\
							break;	\
					} \
				} \
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		rem = 0; \
		ncnt = 0; \
		k = i; \
	} while (0)

#define ANALYTICAL_AVG_INT_ALL_ROWS(TPE)	\
	do { \
		TPE avg = 0; \
		for (; j < i; j++) { \
			TPE v = bp[j]; \
			if (!is_##TPE##_nil(v))	\
				AVERAGE_ITER(TPE, v, avg, rem, ncnt); \
		} \
		if (ncnt == 0) {	\
			for (; k < i; k++)	\
				rb[k] = TPE##_nil; \
			has_nils = true; \
		} else { \
			ANALYTICAL_AVERAGE_INT_CALC_FINALIZE(avg, rem, ncnt); \
			for (; k < i; k++)	\
				rb[k] = avg; \
		} \
		rem = 0; \
		ncnt = 0; \
	} while (0)

#define ANALYTICAL_AVG_INT_CURRENT_ROW(TPE)	\
	do {								\
		for (; k < i; k++) { \
			TPE v = bp[k]; \
			rb[k] = bp[k]; \
			has_nils |= is_##TPE##_nil(v); \
		} \
	} while (0)

#define ANALYTICAL_AVG_INT_OTHERS(TPE)			\
	do {								\
		TPE avg = 0; \
		for (; k < i; k++) {			\
			TPE *bs = bp + start[k], *be = bp + end[k];				\
			for (; bs < be; bs++) {				\
				TPE v = *bs;				\
				if (!is_##TPE##_nil(v))	\
					AVERAGE_ITER(TPE, v, avg, rem, ncnt); \
			}	\
			if (ncnt == 0) {	\
				has_nils = true; \
				rb[k] = TPE##_nil; \
			} else { \
				ANALYTICAL_AVERAGE_INT_CALC_FINALIZE(avg, rem, ncnt); \
				rb[k] = avg; \
			} \
			rem = 0; \
			ncnt = 0; \
		}	\
	} while (0)

#define ANALYTICAL_AVG_INT_PARTITIONS(TPE, IMP)		\
	do {						\
		TPE *bp = (TPE*)Tloc(b, 0), *restrict rb = (TPE *) Tloc(r, 0); \
		if (p) {					\
			for (; i < cnt; i++) {		\
				if (np[i]) 			\
					IMP(TPE);	\
			}						\
		}	\
		i = cnt;			\
		IMP(TPE);	\
	} while (0)

#ifdef HAVE_HGE
#define ANALYTICAL_AVG_INT_LIMIT(IMP) \
	case TYPE_hge: \
		ANALYTICAL_AVG_INT_PARTITIONS(hge, ANALYTICAL_AVG_INT_##IMP); \
		break;
#else
#define ANALYTICAL_AVG_INT_LIMIT(IMP)
#endif

#define ANALYTICAL_AVG_INT_BRANCHES(IMP)		\
	do { \
		switch (tpe) {	\
		case TYPE_bte:	\
			ANALYTICAL_AVG_INT_PARTITIONS(bte, ANALYTICAL_AVG_INT_##IMP);	\
			break;	\
		case TYPE_sht:	\
			ANALYTICAL_AVG_INT_PARTITIONS(sht, ANALYTICAL_AVG_INT_##IMP);	\
			break;	\
		case TYPE_int:	\
			ANALYTICAL_AVG_INT_PARTITIONS(int, ANALYTICAL_AVG_INT_##IMP);	\
			break;	\
		case TYPE_lng:	\
			ANALYTICAL_AVG_INT_PARTITIONS(lng, ANALYTICAL_AVG_INT_##IMP);	\
			break;	\
		ANALYTICAL_AVG_INT_LIMIT(IMP)	\
		default:	\
			goto nosupport;	\
		}	\
	} while (0)

gdk_return
GDKanalyticalavginteger(BAT *r, BAT *p, BAT *o, BAT *b, BAT *s, BAT *e, int tpe, int frame_type)
{
	bool has_nils = false;
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;
	lng rem = 0, ncnt = 0;
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL;

	if (cnt > 0) {
		switch (frame_type) {
		case 3: /* unbounded until current row */	{
			ANALYTICAL_AVG_INT_BRANCHES(UNBOUNDED_TILL_CURRENT_ROW);
		} break;
		case 4: /* current row until unbounded */	{
			ANALYTICAL_AVG_INT_BRANCHES(CURRENT_ROW_TILL_UNBOUNDED);
		} break;
		case 5: /* all rows */	{
			ANALYTICAL_AVG_INT_BRANCHES(ALL_ROWS);
		} break;
		case 6: /* current row */ {
			ANALYTICAL_AVG_INT_BRANCHES(CURRENT_ROW);
		} break;
		default: {
			ANALYTICAL_AVG_INT_BRANCHES(OTHERS);
		}
		}
	}

	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
      nosupport:
	GDKerror("42000!average of type %s to %s unsupported.\n", ATOMname(tpe), ATOMname(tpe));
	return GDK_FAIL;
}

#define ANALYTICAL_STDEV_VARIANCE_UNBOUNDED_TILL_CURRENT_ROW(TPE, SAMPLE, OP)	\
	do { \
		TPE *bp = (TPE*)Tloc(b, 0); \
		for (; k < i;) { \
			j = k; \
			do {	\
				TPE v = bp[k]; \
				if (!is_##TPE##_nil(v))	 {	\
					n++;				\
					delta = (dbl) v - mean;		\
					mean += delta / n;		\
					m2 += delta * ((dbl) v - mean);	\
				}	\
				k++; \
			} while (k < i && !op[k]);	\
			if (isinf(m2)) {	\
				goto overflow;		\
			} else if (n > SAMPLE) { \
				for (; j < k; j++) \
					rb[j] = OP; \
			} else { \
				for (; j < k; j++) \
					rb[j] = dbl_nil; \
				has_nils = true; \
			} \
		} \
		n = 0;	\
		mean = 0;	\
		m2 = 0; \
	} while (0)

#define ANALYTICAL_STDEV_VARIANCE_CURRENT_ROW_TILL_UNBOUNDED(TPE, SAMPLE, OP)	\
	do { \
		TPE *bp = (TPE*)Tloc(b, 0); \
		l = i - 1; \
		for (j = l; ; j--) { \
			TPE v = bp[j]; \
			if (!is_##TPE##_nil(v))	{	\
				n++;				\
				delta = (dbl) v - mean;		\
				mean += delta / n;		\
				m2 += delta * ((dbl) v - mean);	\
			}	\
			if (op[j] || j == k) {	\
				if (isinf(m2)) {	\
					goto overflow;		\
				} else if (n > SAMPLE) { \
					for (; ; l--) { \
						rb[l] = OP; \
						if (l == j)	\
							break;	\
					} \
				} else { \
					for (; ; l--) { \
						rb[l] = dbl_nil; \
						if (l == j)	\
							break;	\
					} \
					has_nils = true; \
				} \
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		n = 0;	\
		mean = 0;	\
		m2 = 0; \
		k = i; \
	} while (0)

#define ANALYTICAL_STDEV_VARIANCE_ALL_ROWS(TPE, SAMPLE, OP)	\
	do { \
		TPE *bp = (TPE*)Tloc(b, 0); \
		for (; j < i; j++) { \
			TPE v = bp[j]; \
			if (is_##TPE##_nil(v))		\
				continue;		\
			n++;				\
			delta = (dbl) v - mean;		\
			mean += delta / n;		\
			m2 += delta * ((dbl) v - mean);	\
		} \
		if (isinf(m2)) {	\
			goto overflow;		\
		} else if (n > SAMPLE) { \
			for (; k < i; k++) \
				rb[k] = OP; \
		} else { \
			for (; k < i; k++) \
				rb[k] = dbl_nil; \
			has_nils = true; \
		} \
		n = 0;	\
		mean = 0;	\
		m2 = 0; \
	} while (0)

#define ANALYTICAL_STDEV_VARIANCE_CURRENT_ROW(TPE, SAMPLE, OP)	\
	do {								\
		for (; k < i; k++) \
			rb[k] = SAMPLE == 1 ? dbl_nil : 0;	\
		has_nils = is_dbl_nil(rb[k - 1]); \
	} while (0)

#define ANALYTICAL_STDEV_VARIANCE_OTHERS(TPE, SAMPLE, OP)	\
	do {								\
		TPE *bp = (TPE*)Tloc(b, 0); \
		for (; k < i; k++) {			\
			TPE *bs = bp + start[k], *be = bp + end[k];		\
			for (; bs < be; bs++) {				\
				TPE v = *bs;				\
				if (is_##TPE##_nil(v))		\
					continue;		\
				n++;				\
				delta = (dbl) v - mean;		\
				mean += delta / n;		\
				m2 += delta * ((dbl) v - mean);	\
			}						\
			if (isinf(m2)) {	\
				goto overflow;		\
			} else if (n > SAMPLE) { \
				rb[k] = OP; \
			} else { \
				rb[k] = dbl_nil; \
				has_nils = true; \
			} \
			n = 0;	\
			mean = 0;	\
			m2 = 0; \
		}	\
	} while (0)

#define ANALYTICAL_STATISTICS_PARTITIONS(TPE, SAMPLE, OP, IMP)		\
	do {						\
		if (p) {					\
			for (; i < cnt; i++) {		\
				if (np[i]) 			\
					IMP(TPE, SAMPLE, OP);	\
			}						\
		}	\
		i = cnt;			\
		IMP(TPE, SAMPLE, OP);	\
	} while (0)

#ifdef HAVE_HGE
#define ANALYTICAL_STATISTICS_LIMIT(IMP, SAMPLE, OP) \
	case TYPE_hge: \
		ANALYTICAL_STATISTICS_PARTITIONS(hge, SAMPLE, OP, ANALYTICAL_##IMP); \
		break;
#else
#define ANALYTICAL_STATISTICS_LIMIT(IMP, SAMPLE, OP)
#endif

#define ANALYTICAL_STATISTICS_BRANCHES(IMP, SAMPLE, OP)		\
	do { \
		switch (tpe) {	\
		case TYPE_bte:	\
			ANALYTICAL_STATISTICS_PARTITIONS(bte, SAMPLE, OP, ANALYTICAL_##IMP);	\
			break;	\
		case TYPE_sht:	\
			ANALYTICAL_STATISTICS_PARTITIONS(sht, SAMPLE, OP, ANALYTICAL_##IMP);	\
			break;	\
		case TYPE_int:	\
			ANALYTICAL_STATISTICS_PARTITIONS(int, SAMPLE, OP, ANALYTICAL_##IMP);	\
			break;	\
		case TYPE_lng:	\
			ANALYTICAL_STATISTICS_PARTITIONS(lng, SAMPLE, OP, ANALYTICAL_##IMP);	\
			break;	\
		case TYPE_flt:	\
			ANALYTICAL_STATISTICS_PARTITIONS(flt, SAMPLE, OP, ANALYTICAL_##IMP);	\
			break;	\
		case TYPE_dbl:	\
			ANALYTICAL_STATISTICS_PARTITIONS(dbl, SAMPLE, OP, ANALYTICAL_##IMP);	\
			break;	\
		ANALYTICAL_STATISTICS_LIMIT(IMP, SAMPLE, OP)	\
		default:	\
			goto nosupport;	\
		}	\
	} while (0)


#define GDK_ANALYTICAL_STDEV_VARIANCE(NAME, SAMPLE, OP, DESC) \
gdk_return \
GDKanalytical_##NAME(BAT *r, BAT *p, BAT *o, BAT *b, BAT *s, BAT *e, int tpe, int frame_type) \
{ \
	bool has_nils = false;	\
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;	\
	lng n = 0;	\
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL;	\
	dbl *restrict rb = (dbl *) Tloc(r, 0), mean = 0, m2 = 0, delta; \
	\
	if (cnt > 0) {	\
		switch (frame_type) {	\
		case 3: /* unbounded until current row */	{	\
			ANALYTICAL_STATISTICS_BRANCHES(STDEV_VARIANCE_UNBOUNDED_TILL_CURRENT_ROW, SAMPLE, OP);	\
		} break;	\
		case 4: /* current row until unbounded */	{	\
			ANALYTICAL_STATISTICS_BRANCHES(STDEV_VARIANCE_CURRENT_ROW_TILL_UNBOUNDED, SAMPLE, OP);	\
		} break;	\
		case 5: /* all rows */	{	\
			ANALYTICAL_STATISTICS_BRANCHES(STDEV_VARIANCE_ALL_ROWS, SAMPLE, OP);	\
		} break;	\
		case 6: /* current row */ {	\
			ANALYTICAL_STATISTICS_BRANCHES(STDEV_VARIANCE_CURRENT_ROW, SAMPLE, OP);	\
		} break;	\
		default: {	\
			ANALYTICAL_STATISTICS_BRANCHES(STDEV_VARIANCE_OTHERS, SAMPLE, OP);	\
		}	\
		}	\
	}	\
	\
	BATsetcount(r, cnt); \
	r->tnonil = !has_nils;	\
	r->tnil = has_nils;	\
	return GDK_SUCCEED; \
  nosupport:	\
	GDKerror("42000!%s of type %s unsupported.\n", DESC, ATOMname(tpe)); \
	return GDK_FAIL; \
  overflow: \
	GDKerror("22003!overflow in calculation.\n"); \
	return GDK_FAIL; \
}

GDK_ANALYTICAL_STDEV_VARIANCE(stddev_samp, 1, sqrt(m2 / (n - 1)), "standard deviation")
GDK_ANALYTICAL_STDEV_VARIANCE(stddev_pop, 0, sqrt(m2 / n), "standard deviation")
GDK_ANALYTICAL_STDEV_VARIANCE(variance_samp, 1, m2 / (n - 1), "variance")
GDK_ANALYTICAL_STDEV_VARIANCE(variance_pop, 0, m2 / n, "variance")

#define ANALYTICAL_COVARIANCE_UNBOUNDED_TILL_CURRENT_ROW(TPE, SAMPLE, OP)	\
	do { \
		TPE *bp1 = (TPE*)Tloc(b1, 0), *bp2 = (TPE*)Tloc(b2, 0); \
		for (; k < i;) { \
			j = k; \
			do {	\
				TPE v1 = bp1[k], v2 = bp2[k]; \
				if (!is_##TPE##_nil(v1) && !is_##TPE##_nil(v2))	{	\
					n++;				\
					delta1 = (dbl) v1 - mean1;		\
					mean1 += delta1 / n;		\
					delta2 = (dbl) v2 - mean2;		\
					mean2 += delta2 / n;		\
					m2 += delta1 * ((dbl) v2 - mean2);	\
				}	\
				k++; \
			} while (k < i && !op[k]);	\
			if (isinf(m2))	\
				goto overflow;		\
			if (n > SAMPLE) { \
				for (; j < k; j++) \
					rb[j] = OP; \
			} else { \
				for (; j < k; j++) \
					rb[j] = dbl_nil; \
				has_nils = true; \
			} \
		} \
		n = 0;	\
		mean1 = 0;	\
		mean2 = 0;	\
		m2 = 0; \
	} while (0)

#define ANALYTICAL_COVARIANCE_CURRENT_ROW_TILL_UNBOUNDED(TPE, SAMPLE, OP)	\
	do { \
		TPE *bp1 = (TPE*)Tloc(b1, 0), *bp2 = (TPE*)Tloc(b2, 0); \
		l = i - 1; \
		for (j = l; ; j--) { \
			TPE v1 = bp1[j], v2 = bp2[j]; \
			if (!is_##TPE##_nil(v1) && !is_##TPE##_nil(v2))	{	\
				n++;				\
				delta1 = (dbl) v1 - mean1;		\
				mean1 += delta1 / n;		\
				delta2 = (dbl) v2 - mean2;		\
				mean2 += delta2 / n;		\
				m2 += delta1 * ((dbl) v2 - mean2);	\
			}	\
			if (op[j] || j == k) {	\
				if (isinf(m2))	\
					goto overflow;		\
				if (n > SAMPLE) { \
					for (; ; l--) { \
						rb[l] = OP; \
						if (l == j)	\
							break;	\
					} \
				} else { \
					for (; ; l--) { \
						rb[l] = dbl_nil; \
						if (l == j)	\
							break;	\
					} \
					has_nils = true; \
				} \
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		n = 0;	\
		mean1 = 0;	\
		mean2 = 0;	\
		m2 = 0; \
		k = i; \
	} while (0)

#define ANALYTICAL_COVARIANCE_ALL_ROWS(TPE, SAMPLE, OP)	\
	do { \
		TPE *bp1 = (TPE*)Tloc(b1, 0), *bp2 = (TPE*)Tloc(b2, 0); \
		for (; j < i; j++) { \
			TPE v1 = bp1[j], v2 = bp2[j]; \
			if (!is_##TPE##_nil(v1) && !is_##TPE##_nil(v2))	{	\
				n++;				\
				delta1 = (dbl) v1 - mean1;		\
				mean1 += delta1 / n;		\
				delta2 = (dbl) v2 - mean2;		\
				mean2 += delta2 / n;		\
				m2 += delta1 * ((dbl) v2 - mean2);	\
			}	\
		} \
		if (isinf(m2))	\
			goto overflow;		\
		if (n > SAMPLE) { \
			for (; k < i; k++) \
				rb[k] = OP; \
		} else { \
			for (; k < i; k++) \
				rb[k] = dbl_nil; \
			has_nils = true; \
		} \
		n = 0;	\
		mean1 = 0;	\
		mean2 = 0;	\
		m2 = 0; \
	} while (0)

#define ANALYTICAL_COVARIANCE_CURRENT_ROW(TPE, SAMPLE, OP)	\
	do {								\
		for (; k < i; k++) \
			rb[k] = SAMPLE == 1 ? dbl_nil : 0;	\
		has_nils = is_dbl_nil(rb[k - 1]); \
	} while (0)

#define ANALYTICAL_COVARIANCE_OTHERS(TPE, SAMPLE, OP)	\
	do {								\
		TPE *bp1 = (TPE*)Tloc(b1, 0), *bp2 = (TPE*)Tloc(b2, 0);	\
		for (; k < i; k++) {		\
			TPE *bs1 = bp1 + start[k];				\
			TPE *be1 = bp1 + end[k];				\
			TPE *bs2 = bp2 + start[k];		\
			for (; bs1 < be1; bs1++, bs2++) {	\
				TPE v1 = *bs1, v2 = *bs2;				\
				if (is_##TPE##_nil(v1) || is_##TPE##_nil(v2))	\
					continue;		\
				n++;				\
				delta1 = (dbl) v1 - mean1;		\
				mean1 += delta1 / n;		\
				delta2 = (dbl) v2 - mean2;		\
				mean2 += delta2 / n;		\
				m2 += delta1 * ((dbl) v2 - mean2);	\
			}	\
			if (isinf(m2))	\
				goto overflow;		\
			if (n > SAMPLE) { \
				rb[k] = OP; \
			} else { \
				rb[k] = dbl_nil; \
				has_nils = true; \
			} \
			n = 0;	\
			mean1 = 0;	\
			mean2 = 0;	\
			m2 = 0; \
		}	\
	} while (0)

#define GDK_ANALYTICAL_COVARIANCE(NAME, SAMPLE, OP) \
gdk_return \
GDKanalytical_##NAME(BAT *r, BAT *p, BAT *o, BAT *b1, BAT *b2, BAT *s, BAT *e, int tpe, int frame_type) \
{ \
	bool has_nils = false;	\
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b1), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;	\
	lng n = 0;	\
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL;	\
	dbl *restrict rb = (dbl *) Tloc(r, 0), mean1 = 0, mean2 = 0, m2 = 0, delta1, delta2; \
	\
	if (cnt > 0) {	\
		switch (frame_type) {	\
		case 3: /* unbounded until current row */	{	\
			ANALYTICAL_STATISTICS_BRANCHES(COVARIANCE_UNBOUNDED_TILL_CURRENT_ROW, SAMPLE, OP);	\
		} break;	\
		case 4: /* current row until unbounded */	{	\
			ANALYTICAL_STATISTICS_BRANCHES(COVARIANCE_CURRENT_ROW_TILL_UNBOUNDED, SAMPLE, OP);	\
		} break;	\
		case 5: /* all rows */	{	\
			ANALYTICAL_STATISTICS_BRANCHES(COVARIANCE_ALL_ROWS, SAMPLE, OP);	\
		} break;	\
		case 6: /* current row */ {	\
			ANALYTICAL_STATISTICS_BRANCHES(COVARIANCE_CURRENT_ROW, SAMPLE, OP);	\
		} break;	\
		default: {	\
			ANALYTICAL_STATISTICS_BRANCHES(COVARIANCE_OTHERS, SAMPLE, OP);	\
		}	\
		}	\
	}	\
	\
	BATsetcount(r, cnt); \
	r->tnonil = !has_nils;	\
	r->tnil = has_nils;	\
	return GDK_SUCCEED; \
  nosupport:	\
	GDKerror("42000!covariance of type %s unsupported.\n", ATOMname(tpe)); \
	return GDK_FAIL; \
  overflow: \
	GDKerror("22003!overflow in calculation.\n"); \
	return GDK_FAIL; \
}

GDK_ANALYTICAL_COVARIANCE(covariance_samp, 1, m2 / (n - 1))
GDK_ANALYTICAL_COVARIANCE(covariance_pop, 0, m2 / n)

#define ANALYTICAL_CORRELATION_UNBOUNDED_TILL_CURRENT_ROW(TPE, SAMPLE, OP)	/* SAMPLE and OP not used */ \
	do { \
		TPE *bp1 = (TPE*)Tloc(b1, 0), *bp2 = (TPE*)Tloc(b2, 0); \
		for (; k < i;) { \
			j = k; \
			do {	\
				TPE v1 = bp1[k], v2 = bp2[k]; \
				if (!is_##TPE##_nil(v1) && !is_##TPE##_nil(v2))	{	\
					n++;	\
					delta1 = (dbl) v1 - mean1;	\
					mean1 += delta1 / n;	\
					delta2 = (dbl) v2 - mean2;	\
					mean2 += delta2 / n;	\
					aux = (dbl) v2 - mean2; \
					up += delta1 * aux;	\
					down1 += delta1 * ((dbl) v1 - mean1);	\
					down2 += delta2 * aux;	\
				}	\
				k++; \
			} while (k < i && !op[k]);	\
			if (isinf(up) || isinf(down1) || isinf(down2))	\
				goto overflow;	\
			if (n != 0 && down1 != 0 && down2 != 0) { \
				res = (up / n) / (sqrt(down1 / n) * sqrt(down2 / n)); \
				assert(!is_dbl_nil(res)); \
			} else { \
				res = dbl_nil; \
				has_nils = true; \
			} \
			for (; j < k; j++) \
				rb[j] = res; \
		} \
		n = 0;	\
		mean1 = 0;	\
		mean2 = 0;	\
		up = 0; \
		down1 = 0;	\
		down2 = 0;	\
	} while (0)

#define ANALYTICAL_CORRELATION_CURRENT_ROW_TILL_UNBOUNDED(TPE, SAMPLE, OP)	/* SAMPLE and OP not used */ \
	do { \
		TPE *bp1 = (TPE*)Tloc(b1, 0), *bp2 = (TPE*)Tloc(b2, 0); \
		l = i - 1; \
		for (j = l; ; j--) { \
			TPE v1 = bp1[j], v2 = bp2[j]; \
			if (!is_##TPE##_nil(v1) && !is_##TPE##_nil(v2))	{	\
				n++;	\
				delta1 = (dbl) v1 - mean1;	\
				mean1 += delta1 / n;	\
				delta2 = (dbl) v2 - mean2;	\
				mean2 += delta2 / n;	\
				aux = (dbl) v2 - mean2; \
				up += delta1 * aux;	\
				down1 += delta1 * ((dbl) v1 - mean1);	\
				down2 += delta2 * aux;	\
			}	\
			if (op[j] || j == k) {	\
				if (isinf(up) || isinf(down1) || isinf(down2))	\
					goto overflow;	\
				if (n != 0 && down1 != 0 && down2 != 0) { \
					res = (up / n) / (sqrt(down1 / n) * sqrt(down2 / n)); \
					assert(!is_dbl_nil(res)); \
				} else { \
					res = dbl_nil; \
					has_nils = true; \
				} \
				for (; ; l--) { \
					rb[l] = res; \
					if (l == j)	\
						break;	\
				} \
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		n = 0;	\
		mean1 = 0;	\
		mean2 = 0;	\
		up = 0; \
		down1 = 0;	\
		down2 = 0;	\
		k = i; \
	} while (0)

#define ANALYTICAL_CORRELATION_ALL_ROWS(TPE, SAMPLE, OP)	/* SAMPLE and OP not used */ \
	do { \
		TPE *bp1 = (TPE*)Tloc(b1, 0), *bp2 = (TPE*)Tloc(b2, 0); \
		for (; j < i; j++) { \
			TPE v1 = bp1[j], v2 = bp2[j]; \
			if (!is_##TPE##_nil(v1) && !is_##TPE##_nil(v2))	{	\
				n++;	\
				delta1 = (dbl) v1 - mean1;	\
				mean1 += delta1 / n;	\
				delta2 = (dbl) v2 - mean2;	\
				mean2 += delta2 / n;	\
				aux = (dbl) v2 - mean2; \
				up += delta1 * aux;	\
				down1 += delta1 * ((dbl) v1 - mean1);	\
				down2 += delta2 * aux;	\
			}	\
		} \
		if (isinf(up) || isinf(down1) || isinf(down2))	\
			goto overflow;	\
		if (n != 0 && down1 != 0 && down2 != 0) { \
			res = (up / n) / (sqrt(down1 / n) * sqrt(down2 / n)); \
			assert(!is_dbl_nil(res)); \
		} else { \
			res = dbl_nil; \
			has_nils = true; \
		} \
		for (; k < i ; k++)	\
			rb[k] = res;	\
		n = 0;	\
		mean1 = 0;	\
		mean2 = 0;	\
		up = 0; \
		down1 = 0;	\
		down2 = 0;	\
	} while (0)

#define ANALYTICAL_CORRELATION_CURRENT_ROW(TPE, SAMPLE, OP)	/* SAMPLE and OP not used */ \
	do {								\
		for (; k < i; k++) \
			rb[k] = dbl_nil;	\
		has_nils = true; \
	} while (0)

#define ANALYTICAL_CORRELATION_OTHERS(TPE, SAMPLE, OP)	/* SAMPLE and OP not used */ \
	do {								\
		TPE *bp1 = (TPE*)Tloc(b1, 0), *bp2 = (TPE*)Tloc(b2, 0);	\
		for (; k < i; k++) {		\
			TPE *bs1 = bp1 + start[i];				\
			TPE *be1 = bp1 + end[i];				\
			TPE *bs2 = bp2 + start[i];		\
			for (; bs1 < be1; bs1++, bs2++) {	\
				TPE v1 = *bs1, v2 = *bs2;				\
				if (is_##TPE##_nil(v1) || is_##TPE##_nil(v2))	\
					continue;		\
				n++;	\
				delta1 = (dbl) v1 - mean1;	\
				mean1 += delta1 / n;	\
				delta2 = (dbl) v2 - mean2;	\
				mean2 += delta2 / n;	\
				aux = (dbl) v2 - mean2; \
				up += delta1 * aux;	\
				down1 += delta1 * ((dbl) v1 - mean1);	\
				down2 += delta2 * aux;	\
			}	\
			if (isinf(up) || isinf(down1) || isinf(down2))	\
				goto overflow;	\
			if (n != 0 && down1 != 0 && down2 != 0) { \
				res = (up / n) / (sqrt(down1 / n) * sqrt(down2 / n)); \
				assert(!is_dbl_nil(res)); \
			} else { \
				res = dbl_nil; \
				has_nils = true; \
			} \
			rb[k] = res;	\
			n = 0;	\
			mean1 = 0;	\
			mean2 = 0;	\
			up = 0; \
			down1 = 0;	\
			down2 = 0;	\
		}	\
	} while (0)

gdk_return
GDKanalytical_correlation(BAT *r, BAT *p, BAT *o, BAT *b1, BAT *b2, BAT *s, BAT *e, int tpe, int frame_type)
{
	bool has_nils = false;
	oid i = 0, j = 0, k = 0, l = 0, cnt = BATcount(b1), *restrict start = s ? (oid*)Tloc(s, 0) : NULL, *restrict end = e ? (oid*)Tloc(e, 0) : NULL;
	lng n = 0;
	bit *np = p ? Tloc(p, 0) : NULL, *op = o ? Tloc(o, 0) : NULL;
	dbl *restrict rb = (dbl *) Tloc(r, 0), mean1 = 0, mean2 = 0, up = 0, down1 = 0, down2 = 0, delta1, delta2, aux, res;

	if (cnt > 0) {
		switch (frame_type) {
		case 3: /* unbounded until current row */	{
			ANALYTICAL_STATISTICS_BRANCHES(CORRELATION_UNBOUNDED_TILL_CURRENT_ROW, ;, ;);
		} break;
		case 4: /* current row until unbounded */	{
			ANALYTICAL_STATISTICS_BRANCHES(CORRELATION_CURRENT_ROW_TILL_UNBOUNDED, ;, ;);
		} break;
		case 5: /* all rows */	{
			ANALYTICAL_STATISTICS_BRANCHES(CORRELATION_ALL_ROWS, ;, ;);
		} break;
		case 6: /* current row */ {
			ANALYTICAL_STATISTICS_BRANCHES(CORRELATION_CURRENT_ROW, ;, ;);
		} break;
		default: {
			ANALYTICAL_STATISTICS_BRANCHES(CORRELATION_OTHERS, ;, ;);
		}
		}
	}

	BATsetcount(r, cnt);
	r->tnonil = !has_nils;
	r->tnil = has_nils;
	return GDK_SUCCEED;
  nosupport:
	GDKerror("42000!correlation of type %s unsupported.\n", ATOMname(tpe));
	return GDK_FAIL;
  overflow:
	GDKerror("22003!overflow in calculation.\n");
	return GDK_FAIL;
}
