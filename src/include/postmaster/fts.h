/*-------------------------------------------------------------------------
 *
 * fts.h
 *	  Interface for fault tolerance service (FTS).
 *
 * Portions Copyright (c) 2005-2010, Greenplum Inc.
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/postmaster/fts.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef FTS_H
#define FTS_H

#include "utils/guc.h"
#include "cdb/cdbutil.h"


/* Queries for FTS messages */
#define	FTS_MSG_PROBE "PROBE"
#define FTS_MSG_SYNCREP_OFF "SYNCREP_OFF"
#define FTS_MSG_PROMOTE "PROMOTE"

#define Natts_fts_message_response 5
#define Anum_fts_message_response_is_mirror_up 0
#define Anum_fts_message_response_is_in_sync 1
#define Anum_fts_message_response_is_syncrep_enabled 2
#define Anum_fts_message_response_is_role_mirror 3
#define Anum_fts_message_response_request_retry 4

#define FTS_MESSAGE_RESPONSE_NTUPLES 1

typedef struct
{
	int16 dbid;
	bool isPrimaryAlive;
	bool isMirrorAlive;
	bool isInSync;
	bool isSyncRepEnabled;
	bool isRoleMirror;
	bool retryRequested;
} probe_result;

/* States used by FTS main loop for probing segments. */
typedef enum
{
	FTS_INITIAL_STATE,

	FTS_PROBE_SEGMENT,     /* ready to send probe message */
	FTS_SYNCREP_SEGMENT,   /* ready to send syncrep off message */
	FTS_PROMOTE_SEGMENT,   /* ready to send promote message */

	FTS_PROBE_SUCCESS,     /* response to probe is ready for processing */
	FTS_SYNCREP_SUCCESS,   /* syncrep was turned off by the primary */
	FTS_PROMOTE_SUCCESS,   /* promotion was triggered on the mirror */

	/*
	 * Failure states are reached as soon as a failure is encountered. The
	 * corresponding message is retried and if the failure persists after max
	 * retries, corrective action is taken by probePublishUpdate().
	 */
	FTS_PROBE_FAILED,      /* the segment should be considered down */
	FTS_SYNCREP_FAILED,    /* XXX double fault? */
	FTS_PROMOTE_FAILED,    /* double fault */

	/* 
	 * This state is reached while processing probe response, e.g. probe failed
	 * to primary whose mirror is already down.  Or if we failed to send
	 * promote message to a mirror whose primary was found down.
	 */
	FTS_DOUBLE_FAULT,
} FtsProbeState;

typedef struct
{
	CdbComponentDatabaseInfo *segment_db_info;
	probe_result result;
	FtsProbeState state;
	GpMonotonicTime startTime;           /* probe start timestamp */
	int16 probe_errno;                   /* saved errno from the latest system call */
	struct pg_conn *conn;                        /* libpq connection object */
	int retry_count;
} probe_response_per_segment;

typedef struct
{
	int num_of_requests; /* number of primaries (with mirror) we want to request */
	probe_response_per_segment *responses;
} fts_context;

typedef struct FtsResponse
{
	bool IsMirrorUp;
	bool IsInSync;
	bool IsSyncRepEnabled;
	bool IsRoleMirror;
	bool RequestRetry;
} FtsResponse;

extern bool am_ftshandler;
extern bool am_mirror;

/*
 * ENUMS
 */

enum probe_result_e
{
	PROBE_DEAD            = 0x00,
	PROBE_ALIVE           = 0x01,
	PROBE_SEGMENT         = 0x02,
	PROBE_FAULT_CRASH     = 0x08,
	PROBE_FAULT_MIRROR    = 0x10,
	PROBE_FAULT_NET       = 0x20,
};

#define PROBE_CHECK_FLAG(result, flag) (((result) & (flag)) == (flag))

#define PROBE_IS_ALIVE(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_ALIVE)
#define PROBE_HAS_FAULT_CRASH(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_FAULT_CRASH)
#define PROBE_HAS_FAULT_MIRROR(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_FAULT_MIRROR)
#define PROBE_HAS_FAULT_NET(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_FAULT_NET)

/*
 * primary/mirror state after probing;
 * this is used to transition the segment pair to next state;
 * we ignore the case where both segments are down, as no transition is performed;
 * each segment can be:
 *    1) (U)p or (D)own
 */
enum probe_transition_e
{
	TRANS_D_D = 0x01,	/* not used for state transitions */
	TRANS_D_U = 0x02,
	TRANS_U_D = 0x04,
	TRANS_U_U = 0x08,

	TRANS_SENTINEL
};

#define IS_VALID_TRANSITION(trans) \
	(trans == TRANS_D_D || trans == TRANS_D_U || trans == TRANS_U_D || trans == TRANS_U_U)

/* buffer size for SQL command */
#define SQL_CMD_BUF_SIZE     1024

/*
 * STRUCTURES
 */

/* prototype */
struct CdbComponentDatabaseInfo;

typedef struct
{
	int	dbid;
	int16 segindex;
	uint8 oldStatus;
	uint8 newStatus;
} FtsSegmentStatusChange;

typedef struct
{
	CdbComponentDatabaseInfo *primary;
	CdbComponentDatabaseInfo *mirror;
	uint32 stateNew;
	uint32 statePrimary;
	uint32 stateMirror;
} FtsSegmentPairState;

/*
 * FTS process interface
 */
extern int ftsprobe_start(void);

/*
 * Interface for probing segments
 */
extern void FtsProbeSegments(CdbComponentDatabases *dbs, uint8 *scan_status);

/*
 * Interface for segment state checking
 */
extern bool FtsIsSegmentAlive(CdbComponentDatabaseInfo *segInfo);
extern CdbComponentDatabaseInfo *FtsGetPeerSegment(CdbComponentDatabases *cdbs,
												   int content, int dbid);
/*
 * Interface for checking if FTS is active
 */
extern bool FtsIsActive(void);

/*
 * Interface for WALREP specific checking
 */
extern void HandleFtsMessage(const char* query_string);
extern void FtsWalRepMessageSegments(fts_context *context);

/*
 * If master has requested FTS to shutdown.
 */
#ifdef FAULT_INJECTOR
extern bool IsFtsShudownRequested(void);
#endif
#endif   /* FTS_H */

