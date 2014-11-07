/*-------------------------------------------------------------------------
 *
 * pg_amproc.h
 *	  definition of the system "amproc" relation (pg_amproc)
 *	  along with the relation's initial contents.
 *
 * The amproc table identifies support procedures associated with index
 * operator families and classes.  These procedures can't be listed in pg_amop
 * since they are not the implementation of any indexable operator.
 *
 * The primary key for this table is <amprocfamily, amproclefttype,
 * amprocrighttype, amprocnum>.  The "default" support functions for a
 * particular opclass within the family are those with amproclefttype =
 * amprocrighttype = opclass's opcintype.  These are the ones loaded into the
 * relcache for an index and typically used for internal index operations.
 * Other support functions are typically used to handle cross-type indexable
 * operators with oprleft/oprright matching the entry's amproclefttype and
 * amprocrighttype. The exact behavior depends on the index AM, however, and
 * some don't pay attention to non-default functions at all.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_amproc.h,v 1.63 2007/01/28 16:16:52 neilc Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_AMPROC_H
#define PG_AMPROC_H

#include "catalog/genbki.h"

/* TIDYCAT_BEGINFAKEDEF

   CREATE TABLE pg_amproc
   with (camelcase=AccessMethodProcedure, oid=false, relid=2603)
   (
   amprocfamily   oid,
   amproclefttype oid,
   amprocrighttype oid,
   amprocnum      smallint, 
   amproc         regproc
   );

   create unique index on pg_amproc(amprocfamily, amproclefttype, amprocrighttype, amprocnum) with (indexid=2655, CamelCase=AccessMethodProcedure, syscacheid=AMPROCNUM, syscache_nbuckets=64);

   alter table pg_amproc add fk amprocfamily on pg_opfamily(oid);
   alter table pg_amproc add fk amproclefttype on pg_type(oid);
   alter table pg_amproc add fk amprocrighttype on pg_type(oid);
   alter table pg_amproc add fk amproc on pg_proc(oid);

   TIDYCAT_ENDFAKEDEF
*/

/* ----------------
 *		pg_amproc definition.  cpp turns this into
 *		typedef struct FormData_pg_amproc
 * ----------------
 */
#define AccessMethodProcedureRelationId  2603

CATALOG(pg_amproc,2603)
{
	Oid			amprocfamily;		/* the index opfamily this entry is for */
	Oid			amproclefttype;		/* procedure's left input data type */
	Oid			amprocrighttype;	/* procedure's right input data type */
	int2		amprocnum;			/* support procedure index */
	regproc		amproc;				/* OID of the proc */
} FormData_pg_amproc;

/* ----------------
 *		Form_pg_amproc corresponds to a pointer to a tuple with
 *		the format of pg_amproc relation.
 * ----------------
 */
typedef FormData_pg_amproc *Form_pg_amproc;

/* ----------------
 *		compiler constants for pg_amproc
 * ----------------
 */
#define Natts_pg_amproc					5
#define Anum_pg_amproc_amprocfamily		1
#define Anum_pg_amproc_amproclefttype	2
#define Anum_pg_amproc_amprocrighttype	3
#define Anum_pg_amproc_amprocnum		4
#define Anum_pg_amproc_amproc			5

/* ----------------
 *		initial contents of pg_amproc
 * ----------------
 */

/* btree */
DATA(insert (	397   2277 2277 1 382 ));
DATA(insert (	421   702 702 1 357 ));
DATA(insert (	423   1560 1560 1 1596 ));
DATA(insert (	424   16 16 1 1693 ));
DATA(insert (	426   1042 1042 1 1078 ));
DATA(insert (	428   17 17 1 1954 ));
DATA(insert (	429   18 18 1 358 ));
DATA(insert (	434   1082 1082 1 1092 ));
DATA(insert (	434   1082 1114 1 2344 ));
DATA(insert (	434   1082 1184 1 2357 ));
DATA(insert (	434   1114 1114 1 2045 ));
DATA(insert (	434   1114 1082 1 2370 ));
DATA(insert (	434   1114 1184 1 2526 ));
DATA(insert (	434   1184 1184 1 1314 ));
DATA(insert (	434   1184 1082 1 2383 ));
DATA(insert (	434   1184 1114 1 2533 ));
DATA(insert (	1970   700 700 1 354 ));
DATA(insert (	1970   700 701 1 2194 ));
DATA(insert (	1970   701 701 1 355 ));
DATA(insert (	1970   701 700 1 2195 ));
DATA(insert (	1974   869 869 1 926 ));
DATA(insert (	1976   21 21 1 350 ));
DATA(insert (	1976   21 23 1 2190 ));
DATA(insert (	1976   21 20 1 2192 ));
DATA(insert (	1976   23 23 1 351 ));
DATA(insert (	1976   23 20 1 2188 ));
DATA(insert (	1976   23 21 1 2191 ));
DATA(insert (	1976   20 20 1 842 ));
DATA(insert (	1976   20 23 1 2189 ));
DATA(insert (	1976   20 21 1 2193 ));
DATA(insert (	1982   1186 1186 1 1315 ));
DATA(insert (	1984   829 829 1 836 ));
DATA(insert (	1986   19 19 1 359 ));
DATA(insert (	1988   1700 1700 1 1769 ));
DATA(insert (	1989   26 26 1 356 ));
DATA(insert (	1991   30 30 1 404 ));
DATA(insert (	1994   25 25 1 360 ));
DATA(insert (	1996   1083 1083 1 1107 ));
DATA(insert (	2000   1266 1266 1 1358 ));
DATA(insert (	2002   1562 1562 1 1672 ));
DATA(insert (	2095   25 25 1 2166 ));
DATA(insert (	2097   1042 1042 1 2180 ));
DATA(insert (	2098   19 19 1 2187 ));
DATA(insert (	2099   790 790 1  377 ));
DATA(insert (	2233   703 703 1  380 ));
DATA(insert (	2234   704 704 1  381 ));
DATA(insert (	2789   27 27 1 2794 ));
DATA(insert (	7080   3310 3310 1 7081 ));
DATA(insert (	2968   2950 2950 1 2960 ));


/* hash */
DATA(insert (	427   1042 1042 1 1080 ));
DATA(insert (	431   18 18 1 454 ));
DATA(insert (	435   1082 1082 1 450 ));
DATA(insert (	1971   700 700 1 451 ));
DATA(insert (	1971   701 701 1 452 ));
DATA(insert (	1975   869 869 1 422 ));
DATA(insert (	1977   21 21 1 449 ));
DATA(insert (	1977   23 23 1 450 ));
DATA(insert (	1977   20 20 1 949 ));
DATA(insert (	1983   1186 1186 1 1697 ));
DATA(insert (	1985   829 829 1 399 ));
DATA(insert (	1987   19 19 1 455 ));
DATA(insert (	1990   26 26 1 453 ));
DATA(insert (	1992   30 30 1 457 ));
DATA(insert (	1995   25 25 1 400 ));
DATA(insert (	1997   1083 1083 1 452 ));
DATA(insert (	1998   1700 1700 1 432 ));
DATA(insert (	1999   1184 1184 1 452 ));
DATA(insert (	2001   1266 1266 1 1696 ));
DATA(insert (	2040   1114 1114 1 452 ));
DATA(insert (	2222   16 16 1 454 ));
DATA(insert (	2223   17 17 1 456 ));
DATA(insert (	2224   22 22 1 398 ));
DATA(insert (	2225   28 28 1 450 ));
DATA(insert (	2226   29 29 1 450 ));
DATA(insert (	2227   702 702 1 450 ));
DATA(insert (	2228   703 703 1 450 ));
DATA(insert (	2229   25 25 1 456 ));
DATA(insert (	2231   1042 1042 1 456 ));
DATA(insert (	2232   19 19 1 455 ));
DATA(insert (	2235   1033 1033 1 329 ));
DATA(insert (	2969   2950 2950 1 2963 ));


/* gist */
DATA(insert (	2593   603 603 1 2578 ));
DATA(insert (	2593   603 603 2 2583 ));
DATA(insert (	2593   603 603 3 2579 ));
DATA(insert (	2593   603 603 4 2580 ));
DATA(insert (	2593   603 603 5 2581 ));
DATA(insert (	2593   603 603 6 2582 ));
DATA(insert (	2593   603 603 7 2584 ));
DATA(insert (	2594   604 604 1 2585 ));
DATA(insert (	2594   604 604 2 2583 ));
DATA(insert (	2594   604 604 3 2586 ));
DATA(insert (	2594   604 604 4 2580 ));
DATA(insert (	2594   604 604 5 2581 ));
DATA(insert (	2594   604 604 6 2582 ));
DATA(insert (	2594   604 604 7 2584 ));
DATA(insert (	2595   718 718 1 2591 ));
DATA(insert (	2595   718 718 2 2583 ));
DATA(insert (	2595   718 718 3 2592 ));
DATA(insert (	2595   718 718 4 2580 ));
DATA(insert (	2595   718 718 5 2581 ));
DATA(insert (	2595   718 718 6 2582 ));
DATA(insert (	2595   718 718 7 2584 ));

/* gin */
DATA(insert (	2745   1007 1007 1  351 ));
DATA(insert (	2745   1007 1007 2 2743 ));
DATA(insert (	2745   1007 1007 3 2743 ));
DATA(insert (	2745   1007 1007 4 2744 ));
DATA(insert (	2745   1009 1009 1  360 ));
DATA(insert (	2745   1009 1009 2 2743 ));
DATA(insert (	2745   1009 1009 3 2743 ));
DATA(insert (	2745   1009 1009 4 2744 ));
DATA(insert (	2745   1023 1023 1 357 ));
DATA(insert (	2745   1023 1023 2 2743 ));
DATA(insert (	2745   1023 1023 3 2743 ));
DATA(insert (	2745   1023 1023 4 2744 ));
DATA(insert (	2745   1561 1561 1 1596 ));
DATA(insert (	2745   1561 1561 2 2743 ));
DATA(insert (	2745   1561 1561 3 2743 ));
DATA(insert (	2745   1561 1561 4 2744 ));
DATA(insert (	2745   1000 1000 1 1693 ));
DATA(insert (	2745   1000 1000 2 2743 ));
DATA(insert (	2745   1000 1000 3 2743 ));
DATA(insert (	2745   1000 1000 4 2744 ));
DATA(insert (	2745   1014 1014 1 1078 ));
DATA(insert (	2745   1014 1014 2 2743 ));
DATA(insert (	2745   1014 1014 3 2743 ));
DATA(insert (	2745   1014 1014 4 2744 ));
DATA(insert (	2745   1001 1001 1 1954 ));
DATA(insert (	2745   1001 1001 2 2743 ));
DATA(insert (	2745   1001 1001 3 2743 ));
DATA(insert (	2745   1001 1001 4 2744 ));
DATA(insert (	2745   1002 1002 1 358 ));
DATA(insert (	2745   1002 1002 2 2743 ));
DATA(insert (	2745   1002 1002 3 2743 ));
DATA(insert (	2745   1002 1002 4 2744 ));
DATA(insert (	2745   1182 1182 1 1092 ));
DATA(insert (	2745   1182 1182 2 2743 ));
DATA(insert (	2745   1182 1182 3 2743 ));
DATA(insert (	2745   1182 1182 4 2744 ));
DATA(insert (	2745   1021 1021 1 354 ));
DATA(insert (	2745   1021 1021 2 2743 ));
DATA(insert (	2745   1021 1021 3 2743 ));
DATA(insert (	2745   1021 1021 4 2744 ));
DATA(insert (	2745   1022 1022 1 355 ));
DATA(insert (	2745   1022 1022 2 2743 ));
DATA(insert (	2745   1022 1022 3 2743 ));
DATA(insert (	2745   1022 1022 4 2744 ));
DATA(insert (	2745   1041 1041 1 926 ));
DATA(insert (	2745   1041 1041 2 2743 ));
DATA(insert (	2745   1041 1041 3 2743 ));
DATA(insert (	2745   1041 1041 4 2744 ));
DATA(insert (	2745   1005 1005 1 350 ));
DATA(insert (	2745   1005 1005 2 2743 ));
DATA(insert (	2745   1005 1005 3 2743 ));
DATA(insert (	2745   1005 1005 4 2744 ));
DATA(insert (	2745   1016 1016 1 842 ));
DATA(insert (	2745   1016 1016 2 2743 ));
DATA(insert (	2745   1016 1016 3 2743 ));
DATA(insert (	2745   1016 1016 4 2744 ));
DATA(insert (	2745   1187 1187 1 1315 ));
DATA(insert (	2745   1187 1187 2 2743 ));
DATA(insert (	2745   1187 1187 3 2743 ));
DATA(insert (	2745   1187 1187 4 2744 ));
DATA(insert (	2745   1040 1040 1 836 ));
DATA(insert (	2745   1040 1040 2 2743 ));
DATA(insert (	2745   1040 1040 3 2743 ));
DATA(insert (	2745   1040 1040 4 2744 ));
DATA(insert (	2745   1003 1003 1 359 ));
DATA(insert (	2745   1003 1003 2 2743 ));
DATA(insert (	2745   1003 1003 3 2743 ));
DATA(insert (	2745   1003 1003 4 2744 ));
DATA(insert (	2745   1231 1231 1 1769 ));
DATA(insert (	2745   1231 1231 2 2743 ));
DATA(insert (	2745   1231 1231 3 2743 ));
DATA(insert (	2745   1231 1231 4 2744 ));
DATA(insert (	2745   1028 1028 1 356 ));
DATA(insert (	2745   1028 1028 2 2743 ));
DATA(insert (	2745   1028 1028 3 2743 ));
DATA(insert (	2745   1028 1028 4 2744 ));
DATA(insert (	2745   1013 1013 1 404 ));
DATA(insert (	2745   1013 1013 2 2743 ));
DATA(insert (	2745   1013 1013 3 2743 ));
DATA(insert (	2745   1013 1013 4 2744 ));
DATA(insert (	2745   1183 1183 1 1107 ));
DATA(insert (	2745   1183 1183 2 2743 ));
DATA(insert (	2745   1183 1183 3 2743 ));
DATA(insert (	2745   1183 1183 4 2744 ));
DATA(insert (	2745   1185 1185 1 1314 ));
DATA(insert (	2745   1185 1185 2 2743 ));
DATA(insert (	2745   1185 1185 3 2743 ));
DATA(insert (	2745   1185 1185 4 2744 ));
DATA(insert (	2745   1270 1270 1 1358 ));
DATA(insert (	2745   1270 1270 2 2743 ));
DATA(insert (	2745   1270 1270 3 2743 ));
DATA(insert (	2745   1270 1270 4 2744 ));
DATA(insert (	2745   1563 1563 1 1672 ));
DATA(insert (	2745   1563 1563 2 2743 ));
DATA(insert (	2745   1563 1563 3 2743 ));
DATA(insert (	2745   1563 1563 4 2744 ));
DATA(insert (	2745   1115 1115 1 2045 ));
DATA(insert (	2745   1115 1115 2 2743 ));
DATA(insert (	2745   1115 1115 3 2743 ));
DATA(insert (	2745   1115 1115 4 2744 ));
DATA(insert (	2745   791 791 1 377 ));
DATA(insert (	2745   791 791 2 2743 ));
DATA(insert (	2745   791 791 3 2743 ));
DATA(insert (	2745   791 791 4 2744 ));
DATA(insert (	2745   1024 1024 1 380 ));
DATA(insert (	2745   1024 1024 2 2743 ));
DATA(insert (	2745   1024 1024 3 2743 ));
DATA(insert (	2745   1024 1024 4 2744 ));
DATA(insert (	2745   1025 1025 1 381 ));
DATA(insert (	2745   1025 1025 2 2743 ));
DATA(insert (	2745   1025 1025 3 2743 ));
DATA(insert (	2745   1025 1025 4 2744 ));

/*
 * the operator routines for the on-disk bitmap index.
 */
DATA(insert (	3014	702 702 1 357 ));		/* abstime */
DATA(insert (	3015	2277  2277 1 382 ));		/* array */
DATA(insert (	3016	1560  1560 1 1596 ));	/* bit */
DATA(insert (	3017	16  16 1 1693 ));	/* bool */
DATA(insert (	3018	1042  1042 1 1078 ));	/* bpchar */
DATA(insert (	3019	17  17 1 1954 ));	/* bytea */
DATA(insert (	3020	 18   18 1 358 ));		/* char */
DATA(insert (	3022	1082  1082 1 1092 ));	/* date */
DATA(insert (	3022 1082 1114 1 2344 ));	/* date-timestamp */
DATA(insert (	3022 1082 1184 1 2357 ));	/* date-timestamptz */
DATA(insert (	3023	700  700 1 354 ));		/* float4 */
DATA(insert (	3023  700 701 1 2194 ));	/* float48 */
DATA(insert (	3024	701  701 1 355 ));		/* float8 */
DATA(insert (	3024  701 700 1 2195 ));	/* float84 */
DATA(insert (	3025	869  869 1  926 ));	/* inet */
DATA(insert (	3026	 21   21 1 350 ));		/* int2 */
DATA(insert (	3026   21 23 1 2190 ));	/* int24 */
DATA(insert (	3026   21 20 1 2192 ));	/* int28 */
DATA(insert (	3027	 23   23 1 351 ));		/* int4 */
DATA(insert (	3027   23 20 1 2191 ));	/* int42 */
DATA(insert (	3027   23 21 1 2188 ));	/* int48 */
DATA(insert (	3028	 20   20 1 842 ));		/* int8 */
DATA(insert (	3028   20 21 1 2193 ));	/* int82 */
DATA(insert (	3028   20 23 1 2189 ));	/* int84 */
DATA(insert (	3029	1186  1186 1 1315 ));	/* interval */
DATA(insert (	3030	 829   829 1  836 ));	/* macaddr */
DATA(insert (	3031	 19   19 1 359 ));		/* name */
DATA(insert (	3032	1700  1700 1 1769 ));	/* numeric */
DATA(insert (	3033	 26   26 1 356 ));		/* oid */
DATA(insert (	3034	 30   30 1 404 ));		/* oidvector */
DATA(insert (	3035	 25   25 1 360 ));		/* text */
DATA(insert (	3036	1083  1083 1 1107 ));	/* time */
DATA(insert (	3037	1184  1184 1 1314 ));	/* timestamptz */
DATA(insert (	3037 1184 1082 1 2383 ));	/* timestamptz-date */
DATA(insert (	3037 1184 1114 1 2533 ));	/* timestamptz-timestamp */
DATA(insert (	3038	1266  1266 1 1358 ));	/* timetz */
DATA(insert (	3039	1562  1562 1 1672 ));	/* varbit */
DATA(insert (	3041	1114  1114 1 2045 ));	/* timestamp */
DATA(insert (	3041 1114 1082 1 2370 ));	/* timestamp-date */
DATA(insert (	3041 1114 1184 1 2526 ));	/* timestamp-timestamptz */
DATA(insert (	3042	 25   25 1 2166 ));	/* text pattern */
DATA(insert (	3044	1042  1042 1 2180 ));	/* bpchar pattern */
DATA(insert (	3045	 19   19 1 2187 ));	/* name pattern */
DATA(insert (	3046	790  790 1  377 ));	/* money */
DATA(insert (	3047	703  703 1 380 ));		/* reltime */
DATA(insert (	3048	704  704 1 381 ));		/* tinterval */


/* BRIN opclasses */
/* minmax bytea */
DATA(insert (   4064    17    17  1  3383 ));
DATA(insert (   4064    17    17  2  3384 ));
DATA(insert (   4064    17    17  3  3385 ));
DATA(insert (   4064    17    17  4  3386 ));
DATA(insert (   4064    17    17  11 1949 ));
DATA(insert (   4064    17    17  12 1950 ));
DATA(insert (   4064    17    17  13 1952 ));
DATA(insert (   4064    17    17  14 1951 ));
/* minmax "char" */
DATA(insert (   4062    18    18  1  3383 ));
DATA(insert (   4062    18    18  2  3384 ));
DATA(insert (   4062    18    18  3  3385 ));
DATA(insert (   4062    18    18  4  3386 ));
DATA(insert (   4062    18    18  11 1246 ));
DATA(insert (   4062    18    18  12   72 ));
DATA(insert (   4062    18    18  13   74 ));
DATA(insert (   4062    18    18  14   73 ));
/* minmax name */
DATA(insert (   4065    19    19  1  3383 ));
DATA(insert (   4065    19    19  2  3384 ));
DATA(insert (   4065    19    19  3  3385 ));
DATA(insert (   4065    19    19  4  3386 ));
DATA(insert (   4065    19    19  11  655 ));
DATA(insert (   4065    19    19  12  656 ));
DATA(insert (   4065    19    19  13  658 ));
DATA(insert (   4065    19    19  14  657 ));
/* minmax bigint */
DATA(insert (   4063    20    20  1  3383 ));
DATA(insert (   4063    20    20  2  3384 ));
DATA(insert (   4063    20    20  3  3385 ));
DATA(insert (   4063    20    20  4  3386 ));
DATA(insert (   4063    20    20  11  469 ));
DATA(insert (   4063    20    20  12  471 ));
DATA(insert (   4063    20    20  13  472 ));
DATA(insert (   4063    20    20  14  470 ));
/* minmax smallint */
DATA(insert (   4067    21    21  1  3383 ));
DATA(insert (   4067    21    21  2  3384 ));
DATA(insert (   4067    21    21  3  3385 ));
DATA(insert (   4067    21    21  4  3386 ));
DATA(insert (   4067    21    21  11   64 ));
DATA(insert (   4067    21    21  12  148 ));
DATA(insert (   4067    21    21  13  151 ));
DATA(insert (   4067    21    21  14  146 ));
/* minmax integer */
DATA(insert (   4054    23    23  1  3383 ));
DATA(insert (   4054    23    23  2  3384 ));
DATA(insert (   4054    23    23  3  3385 ));
DATA(insert (   4054    23    23  4  3386 ));
DATA(insert (   4054    23    23  11   66 ));
DATA(insert (   4054    23    23  12  149 ));
DATA(insert (   4054    23    23  13  150 ));
DATA(insert (   4054    23    23  14  147 ));
/* minmax text */
DATA(insert (   4056    25    25  1  3383 ));
DATA(insert (   4056    25    25  2  3384 ));
DATA(insert (   4056    25    25  3  3385 ));
DATA(insert (   4056    25    25  4  3386 ));
DATA(insert (   4056    25    25  11  740 ));
DATA(insert (   4056    25    25  12  741 ));
DATA(insert (   4056    25    25  13  743 ));
DATA(insert (   4056    25    25  14  742 ));
/* minmax oid */
DATA(insert (   4068    26    26  1  3383 ));
DATA(insert (   4068    26    26  2  3384 ));
DATA(insert (   4068    26    26  3  3385 ));
DATA(insert (   4068    26    26  4  3386 ));
DATA(insert (   4068    26    26  11  716 ));
DATA(insert (   4068    26    26  12  717 ));
DATA(insert (   4068    26    26  13 1639 ));
DATA(insert (   4068    26    26  14 1638 ));
/* minmax tid */
DATA(insert (   4069    27    27  1  3383 ));
DATA(insert (   4069    27    27  2  3384 ));
DATA(insert (   4069    27    27  3  3385 ));
DATA(insert (   4069    27    27  4  3386 ));
DATA(insert (   4069    27    27  11 2791 ));
DATA(insert (   4069    27    27  12 2793 ));
DATA(insert (   4069    27    27  13 2792 ));
DATA(insert (   4069    27    27  14 2790 ));
/* minmax real */
DATA(insert (   4070   700   700  1  3383 ));
DATA(insert (   4070   700   700  2  3384 ));
DATA(insert (   4070   700   700  3  3385 ));
DATA(insert (   4070   700   700  4  3386 ));
DATA(insert (   4070   700   700  11  289 ));
DATA(insert (   4070   700   700  12  290 ));
DATA(insert (   4070   700   700  13  292 ));
DATA(insert (   4070   700   700  14  291 ));
/* minmax double precision */
DATA(insert (   4071   701   701  1  3383 ));
DATA(insert (   4071   701   701  2  3384 ));
DATA(insert (   4071   701   701  3  3385 ));
DATA(insert (   4071   701   701  4  3386 ));
DATA(insert (   4071   701   701  11  295 ));
DATA(insert (   4071   701   701  12  296 ));
DATA(insert (   4071   701   701  13  298 ));
DATA(insert (   4071   701   701  14  297 ));
/* minmax abstime */
DATA(insert (   4072   702   702  1  3383 ));
DATA(insert (   4072   702   702  2  3384 ));
DATA(insert (   4072   702   702  3  3385 ));
DATA(insert (   4072   702   702  4  3386 ));
DATA(insert (   4072   702   702  11  253 ));
DATA(insert (   4072   702   702  12  255 ));
DATA(insert (   4072   702   702  13  256 ));
DATA(insert (   4072   702   702  14  254 ));
/* minmax reltime */
DATA(insert (   4073   703   703  1  3383 ));
DATA(insert (   4073   703   703  2  3384 ));
DATA(insert (   4073   703   703  3  3385 ));
DATA(insert (   4073   703   703  4  3386 ));
DATA(insert (   4073   703   703  11  259 ));
DATA(insert (   4073   703   703  12  261 ));
DATA(insert (   4073   703   703  13  262 ));
DATA(insert (   4073   703   703  14  260 ));
/* minmax macaddr */
DATA(insert (   4074   829   829  1  3383 ));
DATA(insert (   4074   829   829  2  3384 ));
DATA(insert (   4074   829   829  3  3385 ));
DATA(insert (   4074   829   829  4  3386 ));
DATA(insert (   4074   829   829  11  831 ));
DATA(insert (   4074   829   829  12  832 ));
DATA(insert (   4074   829   829  13  834 ));
DATA(insert (   4074   829   829  14  833 ));
/* minmax inet */
DATA(insert (   4075   869   869  1  3383 ));
DATA(insert (   4075   869   869  2  3384 ));
DATA(insert (   4075   869   869  3  3385 ));
DATA(insert (   4075   869   869  4  3386 ));
DATA(insert (   4075   869   869  11  921 ));
DATA(insert (   4075   869   869  12  922 ));
DATA(insert (   4075   869   869  13  924 ));
DATA(insert (   4075   869   869  14  923 ));
/* minmax character */
DATA(insert (   4076  1042  1042  1  3383 ));
DATA(insert (   4076  1042  1042  2  3384 ));
DATA(insert (   4076  1042  1042  3  3385 ));
DATA(insert (   4076  1042  1042  4  3386 ));
DATA(insert (   4076  1042  1042  11 1049 ));
DATA(insert (   4076  1042  1042  12 1050 ));
DATA(insert (   4076  1042  1042  13 1052 ));
DATA(insert (   4076  1042  1042  14 1051 ));
/* minmax date */
DATA(insert (   4061  1082  1082  1  3383 ));
DATA(insert (   4061  1082  1082  2  3384 ));
DATA(insert (   4061  1082  1082  3  3385 ));
DATA(insert (   4061  1082  1082  4  3386 ));
DATA(insert (   4061  1082  1082  11 1087 ));
DATA(insert (   4061  1082  1082  12 1088 ));
DATA(insert (   4061  1082  1082  13 1090 ));
DATA(insert (   4061  1082  1082  14 1089 ));
/* minmax time without time zone */
DATA(insert (   4077  1083  1083  1  3383 ));
DATA(insert (   4077  1083  1083  2  3384 ));
DATA(insert (   4077  1083  1083  3  3385 ));
DATA(insert (   4077  1083  1083  4  3386 ));
DATA(insert (   4077  1083  1083  11 1102 ));
DATA(insert (   4077  1083  1083  12 1103 ));
DATA(insert (   4077  1083  1083  13 1105 ));
DATA(insert (   4077  1083  1083  14 1104 ));
/* minmax timestamp without time zone */
DATA(insert (   4059  1114  1114  1  3383 ));
DATA(insert (   4059  1114  1114  2  3384 ));
DATA(insert (   4059  1114  1114  3  3385 ));
DATA(insert (   4059  1114  1114  4  3386 ));
DATA(insert (   4059  1114  1114  11 2054 ));
DATA(insert (   4059  1114  1114  12 2055 ));
DATA(insert (   4059  1114  1114  13 2056 ));
DATA(insert (   4059  1114  1114  14 2057 ));
/* minmax timestamp with time zone */
DATA(insert (   4060  1184  1184  1  3383 ));
DATA(insert (   4060  1184  1184  2  3384 ));
DATA(insert (   4060  1184  1184  3  3385 ));
DATA(insert (   4060  1184  1184  4  3386 ));
DATA(insert (   4060  1184  1184  11 1154 ));
DATA(insert (   4060  1184  1184  12 1155 ));
DATA(insert (   4060  1184  1184  13 1156 ));
DATA(insert (   4060  1184  1184  14 1157 ));
/* minmax interval */
DATA(insert (   4078  1186  1186  1  3383 ));
DATA(insert (   4078  1186  1186  2  3384 ));
DATA(insert (   4078  1186  1186  3  3385 ));
DATA(insert (   4078  1186  1186  4  3386 ));
DATA(insert (   4078  1186  1186  11 1164 ));
DATA(insert (   4078  1186  1186  12 1165 ));
DATA(insert (   4078  1186  1186  13 1166 ));
DATA(insert (   4078  1186  1186  14 1167 ));
/* minmax time with time zone */
DATA(insert (   4058  1266  1266  1  3383 ));
DATA(insert (   4058  1266  1266  2  3384 ));
DATA(insert (   4058  1266  1266  3  3385 ));
DATA(insert (   4058  1266  1266  4  3386 ));
DATA(insert (   4058  1266  1266  11 1354 ));
DATA(insert (   4058  1266  1266  12 1355 ));
DATA(insert (   4058  1266  1266  13 1356 ));
DATA(insert (   4058  1266  1266  14 1357 ));
/* minmax bit */
DATA(insert (   4079  1560  1560  1  3383 ));
DATA(insert (   4079  1560  1560  2  3384 ));
DATA(insert (   4079  1560  1560  3  3385 ));
DATA(insert (   4079  1560  1560  4  3386 ));
DATA(insert (   4079  1560  1560  11 1595 ));
DATA(insert (   4079  1560  1560  12 1594 ));
DATA(insert (   4079  1560  1560  13 1592 ));
DATA(insert (   4079  1560  1560  14 1593 ));
/* minmax bit varying */
DATA(insert (   4080  1562  1562  1  3383 ));
DATA(insert (   4080  1562  1562  2  3384 ));
DATA(insert (   4080  1562  1562  3  3385 ));
DATA(insert (   4080  1562  1562  4  3386 ));
DATA(insert (   4080  1562  1562  11 1671 ));
DATA(insert (   4080  1562  1562  12 1670 ));
DATA(insert (   4080  1562  1562  13 1668 ));
DATA(insert (   4080  1562  1562  14 1669 ));
/* minmax numeric */
DATA(insert (   4055  1700  1700  1  3383 ));
DATA(insert (   4055  1700  1700  2  3384 ));
DATA(insert (   4055  1700  1700  3  3385 ));
DATA(insert (   4055  1700  1700  4  3386 ));
DATA(insert (   4055  1700  1700  11 1722 ));
DATA(insert (   4055  1700  1700  12 1723 ));
DATA(insert (   4055  1700  1700  13 1721 ));
DATA(insert (   4055  1700  1700  14 1720 ));
/* minmax uuid */
DATA(insert (   4081  2950  2950  1  3383 ));
DATA(insert (   4081  2950  2950  2  3384 ));
DATA(insert (   4081  2950  2950  3  3385 ));
DATA(insert (   4081  2950  2950  4  3386 ));
DATA(insert (   4081  2950  2950  11 2954 ));
DATA(insert (   4081  2950  2950  12 2955 ));
DATA(insert (   4081  2950  2950  13 2957 ));
DATA(insert (   4081  2950  2950  14 2958 ));
/* minmax pg_lsn */
DATA(insert (   4082  3220  3220  1  3383 ));
DATA(insert (   4082  3220  3220  2  3384 ));
DATA(insert (   4082  3220  3220  3  3385 ));
DATA(insert (   4082  3220  3220  4  3386 ));
DATA(insert (   4082  3220  3220  11 3231 ));
DATA(insert (   4082  3220  3220  12 3232 ));
DATA(insert (   4082  3220  3220  13 3234 ));
DATA(insert (   4082  3220  3220  14 3235 ));

#endif   /* PG_AMPROC_H */
