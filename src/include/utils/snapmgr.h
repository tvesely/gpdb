/*-------------------------------------------------------------------------
 *
 * snapmgr.h
 *	  POSTGRES snapshot manager
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/snapmgr.h,v 1.2 2008/05/12 20:02:02 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef SNAPMGR_H
#define SNAPMGR_H

#include "utils/snapshot.h"


extern bool FirstSnapshotSet;

extern TransactionId TransactionXmin;
extern TransactionId RecentXmin;
extern TransactionId RecentGlobalXmin;

extern Snapshot GetTransactionSnapshot(void);
extern Snapshot GetLatestSnapshot(void);
extern void SnapshotSetCommandId(CommandId curcid);

extern void PushActiveSnapshot(Snapshot snapshot);
extern void PushUpdatedSnapshot(Snapshot snapshot);
extern void PopActiveSnapshot(void);
extern Snapshot GetActiveSnapshot(void);
extern bool ActiveSnapshotSet(void);

extern Snapshot RegisterSnapshot(Snapshot snapshot);
extern void UnregisterSnapshot(Snapshot snapshot);

extern void AtSubCommit_Snapshot(int level);
extern void AtSubAbort_Snapshot(int level);
extern void AtEOXact_Snapshot(bool isCommit);

extern void LogDistributedSnapshotInfo(Snapshot snapshot, const char *prefix);

#endif /* SNAPMGR_H */
