/*
 * contrib/intarray/_int_gist.c
 */
#include "postgres.h"

#include "access/gist.h"
#include "access/skey.h"

#include "_int.h"

#define GETENTRY(vec,pos) ((ArrayType *) DatumGetPointer((vec)->vector[(pos)].key))

/*
** GiST support methods
*/
PG_FUNCTION_INFO_V1(g_int_consistent);
PG_FUNCTION_INFO_V1(g_int_compress);
PG_FUNCTION_INFO_V1(g_int_decompress);
PG_FUNCTION_INFO_V1(g_int_penalty);
PG_FUNCTION_INFO_V1(g_int_picksplit);
PG_FUNCTION_INFO_V1(g_int_union);
PG_FUNCTION_INFO_V1(g_int_same);

Datum		g_int_consistent(PG_FUNCTION_ARGS);
Datum		g_int_compress(PG_FUNCTION_ARGS);
Datum		g_int_decompress(PG_FUNCTION_ARGS);
Datum		g_int_penalty(PG_FUNCTION_ARGS);
Datum		g_int_picksplit(PG_FUNCTION_ARGS);
Datum		g_int_union(PG_FUNCTION_ARGS);
Datum		g_int_same(PG_FUNCTION_ARGS);


/*
** The GiST Consistent method for _intments
** Should return false if for all data items x below entry,
** the predicate x op query == FALSE, where op is the oper
** corresponding to strategy in the pg_amop table.
*/
Datum
g_int_consistent(PG_FUNCTION_ARGS)
{
	GISTENTRY  *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
<<<<<<< HEAD
	ArrayType  *query = (ArrayType *) PG_GETARG_ARRAYTYPE_P_COPY(1);
=======
	ArrayType  *query = PG_GETARG_ARRAYTYPE_P_COPY(1);
>>>>>>> a4bebdd92624e018108c2610fc3f2c1584b6c687
	StrategyNumber strategy = (StrategyNumber) PG_GETARG_UINT16(2);

	/* Oid		subtype = PG_GETARG_OID(3); */
	bool	   *recheck = (bool *) PG_GETARG_POINTER(4);
	bool		retval;

	/* this is exact except for RTSameStrategyNumber */
	*recheck = (strategy == RTSameStrategyNumber);

	if (strategy == BooleanSearchStrategy)
	{
		retval = execconsistent((QUERYTYPE *) query,
								(ArrayType *) DatumGetPointer(entry->key),
								GIST_LEAF(entry));

		pfree(query);
		PG_RETURN_BOOL(retval);
	}

	/* sort query for fast search, key is already sorted */
	CHECKARRVALID(query);
	PREPAREARR(query);

	switch (strategy)
	{
		case RTOverlapStrategyNumber:
			retval = inner_int_overlap((ArrayType *) DatumGetPointer(entry->key),
									   query);
			break;
		case RTSameStrategyNumber:
			if (GIST_LEAF(entry))
				DirectFunctionCall3(g_int_same,
									entry->key,
									PointerGetDatum(query),
									PointerGetDatum(&retval));
			else
				retval = inner_int_contains((ArrayType *) DatumGetPointer(entry->key),
											query);
			break;
		case RTContainsStrategyNumber:
		case RTOldContainsStrategyNumber:
			retval = inner_int_contains((ArrayType *) DatumGetPointer(entry->key),
										query);
			break;
		case RTContainedByStrategyNumber:
		case RTOldContainedByStrategyNumber:
			if (GIST_LEAF(entry))
				retval = inner_int_contains(query,
								  (ArrayType *) DatumGetPointer(entry->key));
			else
				retval = inner_int_overlap((ArrayType *) DatumGetPointer(entry->key),
										   query);
			break;
		default:
			retval = FALSE;
	}
	pfree(query);
	PG_RETURN_BOOL(retval);
}

Datum
g_int_union(PG_FUNCTION_ARGS)
{
	GistEntryVector *entryvec = (GistEntryVector *) PG_GETARG_POINTER(0);
	int		   *size = (int *) PG_GETARG_POINTER(1);
	int4		i,
			   *ptr;
	ArrayType  *res;
	int			totlen = 0;

	for (i = 0; i < entryvec->n; i++)
	{
		ArrayType  *ent = GETENTRY(entryvec, i);

		CHECKARRVALID(ent);
		totlen += ARRNELEMS(ent);
	}

	res = new_intArrayType(totlen);
	ptr = ARRPTR(res);

	for (i = 0; i < entryvec->n; i++)
	{
		ArrayType  *ent = GETENTRY(entryvec, i);
		int			nel;

		nel = ARRNELEMS(ent);
		memcpy(ptr, ARRPTR(ent), nel * sizeof(int4));
		ptr += nel;
	}

	QSORT(res, 1);
	res = _int_unique(res);
	*size = VARSIZE(res);
	PG_RETURN_POINTER(res);
}

/*
** GiST Compress and Decompress methods
*/
Datum
g_int_compress(PG_FUNCTION_ARGS)
{
	GISTENTRY  *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
	GISTENTRY  *retval;
	ArrayType  *r;
	int			len;
	int		   *dr;
	int			i,
				min,
				cand;

	if (entry->leafkey)
	{
		r = DatumGetArrayTypePCopy(entry->key);
		CHECKARRVALID(r);
		PREPAREARR(r);

		if (ARRNELEMS(r) >= 2 * MAXNUMRANGE)
			elog(NOTICE, "input array is too big (%d maximum allowed, %d current), use gist__intbig_ops opclass instead",
				 2 * MAXNUMRANGE - 1, ARRNELEMS(r));

		retval = palloc(sizeof(GISTENTRY));
		gistentryinit(*retval, PointerGetDatum(r),
					  entry->rel, entry->page, entry->offset, FALSE);

		PG_RETURN_POINTER(retval);
	}

	/*
	 * leaf entries never compress one more time, only when entry->leafkey
	 * ==true, so now we work only with internal keys
	 */

	r = DatumGetArrayTypeP(entry->key);
	CHECKARRVALID(r);
	if (ARRISEMPTY(r))
	{
		if (r != (ArrayType *) DatumGetPointer(entry->key))
			pfree(r);
		PG_RETURN_POINTER(entry);
	}

	if ((len = ARRNELEMS(r)) >= 2 * MAXNUMRANGE)
	{							/* compress */
		if (r == (ArrayType *) DatumGetPointer(entry->key))
			r = DatumGetArrayTypePCopy(entry->key);
		r = resize_intArrayType(r, 2 * (len));

		dr = ARRPTR(r);

		for (i = len - 1; i >= 0; i--)
			dr[2 * i] = dr[2 * i + 1] = dr[i];

		len *= 2;
		cand = 1;
		while (len > MAXNUMRANGE * 2)
		{
			min = 0x7fffffff;
			for (i = 2; i < len; i += 2)
				if (min > (dr[i] - dr[i - 1]))
				{
					min = (dr[i] - dr[i - 1]);
					cand = i;
				}
			memmove((void *) &dr[cand - 1], (void *) &dr[cand + 1], (len - cand - 1) * sizeof(int32));
			len -= 2;
		}
		r = resize_intArrayType(r, len);
		retval = palloc(sizeof(GISTENTRY));
		gistentryinit(*retval, PointerGetDatum(r),
					  entry->rel, entry->page, entry->offset, FALSE);
		PG_RETURN_POINTER(retval);
	}
	else
		PG_RETURN_POINTER(entry);

	PG_RETURN_POINTER(entry);
}

Datum
g_int_decompress(PG_FUNCTION_ARGS)
{
	GISTENTRY  *entry = (GISTENTRY *) PG_GETARG_POINTER(0);
	GISTENTRY  *retval;
	ArrayType  *r;
	int		   *dr,
				lenr;
	ArrayType  *in;
	int			lenin;
	int		   *din;
	int			i,
				j;

	in = DatumGetArrayTypeP(entry->key);

	CHECKARRVALID(in);
	if (ARRISEMPTY(in))
	{
		if (in != (ArrayType *) DatumGetPointer(entry->key))
		{
			retval = palloc(sizeof(GISTENTRY));
			gistentryinit(*retval, PointerGetDatum(in),
						  entry->rel, entry->page, entry->offset, FALSE);
			PG_RETURN_POINTER(retval);
		}

		PG_RETURN_POINTER(entry);
	}

	lenin = ARRNELEMS(in);

	if (lenin < 2 * MAXNUMRANGE)
	{							/* not compressed value */
		if (in != (ArrayType *) DatumGetPointer(entry->key))
		{
			retval = palloc(sizeof(GISTENTRY));
			gistentryinit(*retval, PointerGetDatum(in),
						  entry->rel, entry->page, entry->offset, FALSE);

			PG_RETURN_POINTER(retval);
		}
		PG_RETURN_POINTER(entry);
	}

	din = ARRPTR(in);
	lenr = internal_size(din, lenin);

	r = new_intArrayType(lenr);
	dr = ARRPTR(r);

	for (i = 0; i < lenin; i += 2)
		for (j = din[i]; j <= din[i + 1]; j++)
			if ((!i) || *(dr - 1) != j)
				*dr++ = j;

	if (in != (ArrayType *) DatumGetPointer(entry->key))
		pfree(in);
	retval = palloc(sizeof(GISTENTRY));
	gistentryinit(*retval, PointerGetDatum(r),
				  entry->rel, entry->page, entry->offset, FALSE);

	PG_RETURN_POINTER(retval);
}

/*
** The GiST Penalty method for _intments
*/
Datum
g_int_penalty(PG_FUNCTION_ARGS)
{
	GISTENTRY  *origentry = (GISTENTRY *) PG_GETARG_POINTER(0);
	GISTENTRY  *newentry = (GISTENTRY *) PG_GETARG_POINTER(1);
	float	   *result = (float *) PG_GETARG_POINTER(2);
	ArrayType  *ud;
	float		tmp1,
				tmp2;

	ud = inner_int_union((ArrayType *) DatumGetPointer(origentry->key),
						 (ArrayType *) DatumGetPointer(newentry->key));
	rt__int_size(ud, &tmp1);
	rt__int_size((ArrayType *) DatumGetPointer(origentry->key), &tmp2);
	*result = tmp1 - tmp2;
	pfree(ud);

	PG_RETURN_POINTER(result);
}



Datum
g_int_same(PG_FUNCTION_ARGS)
{
	ArrayType  *a = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *b = PG_GETARG_ARRAYTYPE_P(1);
	bool	   *result = (bool *) PG_GETARG_POINTER(2);
	int4		n = ARRNELEMS(a);
	int4	   *da,
			   *db;

	CHECKARRVALID(a);
	CHECKARRVALID(b);

	if (n != ARRNELEMS(b))
	{
		*result = false;
		PG_RETURN_POINTER(result);
	}
	*result = TRUE;
	da = ARRPTR(a);
	db = ARRPTR(b);
	while (n--)
	{
		if (*da++ != *db++)
		{
			*result = FALSE;
			break;
		}
	}

	PG_RETURN_POINTER(result);
}

/*****************************************************************
** Common GiST Method
*****************************************************************/

typedef struct
{
	OffsetNumber pos;
	float		cost;
} SPLITCOST;

static int
comparecost(const void *a, const void *b)
{
	if (((SPLITCOST *) a)->cost == ((SPLITCOST *) b)->cost)
		return 0;
	else
		return (((SPLITCOST *) a)->cost > ((SPLITCOST *) b)->cost) ? 1 : -1;
}

/*
** The GiST PickSplit method for _intments
** We use Guttman's poly time split algorithm
*/
Datum
g_int_picksplit(PG_FUNCTION_ARGS)
{
	GistEntryVector *entryvec = (GistEntryVector *) PG_GETARG_POINTER(0);
	GIST_SPLITVEC *v = (GIST_SPLITVEC *) PG_GETARG_POINTER(1);
	OffsetNumber i,
				j;
	ArrayType  *datum_alpha,
			   *datum_beta;
	ArrayType  *datum_l,
			   *datum_r;
	ArrayType  *union_d,
			   *union_dl,
			   *union_dr;
	ArrayType  *inter_d;
	bool		firsttime;
	float		size_alpha,
				size_beta,
				size_union,
				size_inter;
	float		size_waste,
				waste;
	float		size_l,
				size_r;
	int			nbytes;
	OffsetNumber seed_1 = 0,
				seed_2 = 0;
	OffsetNumber *left,
			   *right;
	OffsetNumber maxoff;
	SPLITCOST  *costvector;

#ifdef GIST_DEBUG
	elog(DEBUG3, "--------picksplit %d", entryvec->n);
#endif

	maxoff = entryvec->n - 2;
	nbytes = (maxoff + 2) * sizeof(OffsetNumber);
	v->spl_left = (OffsetNumber *) palloc(nbytes);
	v->spl_right = (OffsetNumber *) palloc(nbytes);

	firsttime = true;
	waste = 0.0;
	for (i = FirstOffsetNumber; i < maxoff; i = OffsetNumberNext(i))
	{
		datum_alpha = GETENTRY(entryvec, i);
		for (j = OffsetNumberNext(i); j <= maxoff; j = OffsetNumberNext(j))
		{
			datum_beta = GETENTRY(entryvec, j);

			/* compute the wasted space by unioning these guys */
			/* size_waste = size_union - size_inter; */
			union_d = inner_int_union(datum_alpha, datum_beta);
			rt__int_size(union_d, &size_union);
			inter_d = inner_int_inter(datum_alpha, datum_beta);
			rt__int_size(inter_d, &size_inter);
			size_waste = size_union - size_inter;

			pfree(union_d);

			if (inter_d != (ArrayType *) NULL)
				pfree(inter_d);

			/*
			 * are these a more promising split that what we've already seen?
			 */

			if (size_waste > waste || firsttime)
			{
				waste = size_waste;
				seed_1 = i;
				seed_2 = j;
				firsttime = false;
			}
		}
	}

	left = v->spl_left;
	v->spl_nleft = 0;
	right = v->spl_right;
	v->spl_nright = 0;
	if (seed_1 == 0 || seed_2 == 0)
	{
		seed_1 = 1;
		seed_2 = 2;
	}

	datum_alpha = GETENTRY(entryvec, seed_1);
	datum_l = copy_intArrayType(datum_alpha);
	rt__int_size(datum_l, &size_l);
	datum_beta = GETENTRY(entryvec, seed_2);
	datum_r = copy_intArrayType(datum_beta);
	rt__int_size(datum_r, &size_r);

	maxoff = OffsetNumberNext(maxoff);

	/*
	 * sort entries
	 */
	costvector = (SPLITCOST *) palloc(sizeof(SPLITCOST) * maxoff);
	for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
	{
		costvector[i - 1].pos = i;
		datum_alpha = GETENTRY(entryvec, i);
		union_d = inner_int_union(datum_l, datum_alpha);
		rt__int_size(union_d, &size_alpha);
		pfree(union_d);
		union_d = inner_int_union(datum_r, datum_alpha);
		rt__int_size(union_d, &size_beta);
		pfree(union_d);
		costvector[i - 1].cost = Abs((size_alpha - size_l) - (size_beta - size_r));
	}
	qsort((void *) costvector, maxoff, sizeof(SPLITCOST), comparecost);

	/*
	 * Now split up the regions between the two seeds.	An important property
	 * of this split algorithm is that the split vector v has the indices of
	 * items to be split in order in its left and right vectors.  We exploit
	 * this property by doing a merge in the code that actually splits the
	 * page.
	 *
	 * For efficiency, we also place the new index tuple in this loop. This is
	 * handled at the very end, when we have placed all the existing tuples
	 * and i == maxoff + 1.
	 */


	for (j = 0; j < maxoff; j++)
	{
		i = costvector[j].pos;

		/*
		 * If we've already decided where to place this item, just put it on
		 * the right list.	Otherwise, we need to figure out which page needs
		 * the least enlargement in order to store the item.
		 */

		if (i == seed_1)
		{
			*left++ = i;
			v->spl_nleft++;
			continue;
		}
		else if (i == seed_2)
		{
			*right++ = i;
			v->spl_nright++;
			continue;
		}

		/* okay, which page needs least enlargement? */
		datum_alpha = GETENTRY(entryvec, i);
		union_dl = inner_int_union(datum_l, datum_alpha);
		union_dr = inner_int_union(datum_r, datum_alpha);
		rt__int_size(union_dl, &size_alpha);
		rt__int_size(union_dr, &size_beta);

		/* pick which page to add it to */
		if (size_alpha - size_l < size_beta - size_r + WISH_F(v->spl_nleft, v->spl_nright, 0.01))
		{
			if (datum_l)
				pfree(datum_l);
			if (union_dr)
				pfree(union_dr);
			datum_l = union_dl;
			size_l = size_alpha;
			*left++ = i;
			v->spl_nleft++;
		}
		else
		{
			if (datum_r)
				pfree(datum_r);
			if (union_dl)
				pfree(union_dl);
			datum_r = union_dr;
			size_r = size_beta;
			*right++ = i;
			v->spl_nright++;
		}
	}
	pfree(costvector);
	*right = *left = FirstOffsetNumber;

	v->spl_ldatum = PointerGetDatum(datum_l);
	v->spl_rdatum = PointerGetDatum(datum_r);

	PG_RETURN_POINTER(v);
}
