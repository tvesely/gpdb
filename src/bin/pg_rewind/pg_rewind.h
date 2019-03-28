/*-------------------------------------------------------------------------
 *
 * pg_rewind.h
 *
 *
 * Portions Copyright (c) 1996-2015, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_REWIND_H
#define PG_REWIND_H

#include "c.h"

#include "datapagemap.h"

#include "access/timeline.h"
#include "storage/block.h"
#include "storage/relfilenode.h"

/* Configuration options */
extern char *datadir_target;
extern char *datadir_source;
extern char *connstr_source;
extern bool debug;
extern bool showprogress;
extern bool dry_run;

extern const char *progname;

extern int32 dbid_target;
extern int32 dbid_source;

/* in parsexlog.c */
extern void extractPageMap(const char *datadir, XLogRecPtr startpoint,
			   TimeLineID tli, XLogRecPtr endpoint);
extern void findLastCheckpoint(const char *datadir, XLogRecPtr searchptr,
				   TimeLineID tli,
				   XLogRecPtr *lastchkptrec, TimeLineID *lastchkpttli,
				   XLogRecPtr *lastchkptredo);
extern XLogRecPtr readOneRecord(const char *datadir, XLogRecPtr ptr,
			  TimeLineID tli);

/* in timeline.c */
extern TimeLineHistoryEntry *rewind_parseTimeLineHistory(char *buffer,
							TimeLineID targetTLI, int *nentries);

extern void set_tablespace_version_directory_dbid(int32 dbid);
#endif   /* PG_REWIND_H */
