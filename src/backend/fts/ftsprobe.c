/*-------------------------------------------------------------------------
 *
 * ftsprobe.c
 *	  Implementation of segment probing interface
 *
 * Portions Copyright (c) 2006-2011, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/fts/ftsprobe.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include <pthread.h>
#include <limits.h>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "libpq-fe.h"
#include "libpq-int.h"

#include "cdb/cdbgang.h"		/* gp_pthread_create */
#include "libpq/ip.h"
#include "postmaster/fts.h"
#include "postmaster/ftsprobe.h"

#include "executor/spi.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

static bool probePollIn(probe_response_per_segment *ftsInfo);

/*
 * Establish async libpq connection to a segment
 */
static bool
ftsConnect(probe_response_per_segment *ftsInfo)
{
	char conninfo[1024];

	snprintf(conninfo, 1024, "host=%s port=%d gpconntype=%s",
			 ftsInfo->segment_db_info->hostip, ftsInfo->segment_db_info->port,
			 GPCONN_TYPE_FTS);
	ftsInfo->conn = PQconnectdb(conninfo);

	if (ftsInfo->conn == NULL)
	{
		elog(ERROR, "FTS: cannot create libpq connection object, possibly out of memory"
			 " (content=%d, dbid=%d)", ftsInfo->segment_db_info->segindex,
			 ftsInfo->segment_db_info->dbid);
	}
	if (ftsInfo->conn->status == CONNECTION_BAD)
	{
		elog(LOG, "FTS: cannot establish libpq connection to (content=%d, dbid=%d): %s",
			 ftsInfo->segment_db_info->segindex, ftsInfo->segment_db_info->dbid,
			 ftsInfo->conn->errorMessage.data);
		return false;
	}
	return true;
}

/*
 * Send FTS query
 */
static bool
ftsSend(probe_response_per_segment *ftsInfo)
{
	const char *message;
	switch (ftsInfo->state)
	{
		case FTS_PROBE_SEGMENT:
			message = FTS_MSG_PROBE;
			break;
		case FTS_SYNCREP_SEGMENT:
			message = FTS_MSG_SYNCREP_OFF;
			break;
		case FTS_PROMOTE_SEGMENT:
			message = FTS_MSG_PROMOTE;
			break;
		default:
			elog(ERROR, "invalid FTS probe state: %d (content=%d, dbid=%d)",
				 ftsInfo->state, ftsInfo->segment_db_info->segindex,
				 ftsInfo->segment_db_info->dbid);
	}
	if (!PQsendQuery(ftsInfo->conn, message))
	{
		elog(LOG, "FTS: failed to send query '%s' to (content=%d, dbid=%d): "
			 "connection status %d, %s", message, ftsInfo->segment_db_info->segindex,
			 ftsInfo->segment_db_info->dbid, ftsInfo->conn->status,
			 ftsInfo->conn->errorMessage.data);
		return false;
	}
	return true;
}

/*
 * Record FTS handler's response from libpq result into probe_result
 */
static void
probeRecordResponse(probe_response_per_segment *ftsInfo, PGresult *result)
{
	ftsInfo->result.isPrimaryAlive = true;

	int *isMirrorAlive = (int *) PQgetvalue(result, 0,
											Anum_fts_message_response_is_mirror_up);
	Assert (isMirrorAlive);
	ftsInfo->result.isMirrorAlive = *isMirrorAlive;

	int *isInSync = (int *) PQgetvalue(result, 0,
									   Anum_fts_message_response_is_in_sync);
	Assert (isInSync);
	ftsInfo->result.isInSync = *isInSync;

	int *isSyncRepEnabled = (int *) PQgetvalue(result, 0,
											   Anum_fts_message_response_is_syncrep_enabled);
	Assert (isSyncRepEnabled);
	ftsInfo->result.isSyncRepEnabled = *isSyncRepEnabled;

	int *isRoleMirror = (int *) PQgetvalue(result, 0,
										   Anum_fts_message_response_is_role_mirror);
	Assert (isRoleMirror);
	ftsInfo->result.isRoleMirror = *isRoleMirror;

	int *retryRequested = (int *) PQgetvalue(result, 0,
											 Anum_fts_message_response_request_retry);
	Assert (retryRequested);
	ftsInfo->result.retryRequested = *retryRequested;

	elog(LOG, "FTS: segment (content=%d, dbid=%d, role=%c) reported "
		 "isMirrorUp %d, isInSync %d, isSyncRepEnabled %d, "
		 "isRoleMirror %d, and retryRequested %d to the prober.",
		 ftsInfo->segment_db_info->segindex,
		 ftsInfo->segment_db_info->dbid,
		 ftsInfo->segment_db_info->role,
		 ftsInfo->result.isMirrorAlive,
		 ftsInfo->result.isInSync,
		 ftsInfo->result.isSyncRepEnabled,
		 ftsInfo->result.isRoleMirror,
		 ftsInfo->result.retryRequested);
}

/*
 * Receive segment response
 */
static bool
ftsReceive(probe_response_per_segment *ftsInfo)
{
	/* last non-null result that will be returned */
	PGresult   *lastResult = NULL;

	/*
	 * Could directly use PQexec() here instead of loop and have very simple
	 * version, only hurdle is PQexec() (and PQgetResult()) has infinite wait
	 * coded in pqWait() and hence doesn't allow for checking the timeout
	 * (SLA) for FTS probe.
	 *
	 * It would have been really easy if pqWait() could have been taken
	 * timeout as connection object property instead of having -1 hard-coded,
	 * that way could have easily used the PQexec() interface.
	 */
	for (;;)
	{
		PGresult   *tmpResult = NULL;
		/*
		 * Receive data until PQgetResult is ready to get the result without
		 * blocking.
		 */
		while (PQisBusy(ftsInfo->conn))
		{
			/*
			 * XXX see if PQexec() can be used instead of this additional
			 * poll(), once we have functioning FTS without threads.
			 */
			if (!probePollIn(ftsInfo))
			{
				return false;	/* either timeout or error happened */
			}

			if (PQconsumeInput(ftsInfo->conn) == 0)
			{
				elog(LOG, "FTS: error reading probe response from "
					 "libpq socket (content=%d, dbid=%d): %s",
					 ftsInfo->segment_db_info->segindex, ftsInfo->segment_db_info->dbid,
					 PQerrorMessage(ftsInfo->conn));
				return false;	/* trouble */
			}
		}

		/*
		 * Emulate the PQexec()'s behavior of returning the last result when
		 * there are many. Since walsender will never generate multiple
		 * results, we skip the concatenation of error messages.
		 */
		tmpResult = PQgetResult(ftsInfo->conn);
		if (tmpResult == NULL)
			break;				/* response received */

		PQclear(lastResult);
		lastResult = tmpResult;

		if (PQstatus(ftsInfo->conn) == CONNECTION_BAD)
		{
			elog(LOG, "FTS: error reading probe response from "
				 "libpq socket (content=%d, dbid=%d): %s",
				 ftsInfo->segment_db_info->segindex, ftsInfo->segment_db_info->dbid,
				 PQerrorMessage(ftsInfo->conn));
			return false;	/* trouble */
		}
	}

	if (PQresultStatus(lastResult) != PGRES_TUPLES_OK)
	{
		PQclear(lastResult);
		elog(LOG, "FTS: error reading probe response from "
			 "libpq socket (content=%d, dbid=%d): %s",
			 ftsInfo->segment_db_info->segindex, ftsInfo->segment_db_info->dbid,
			 PQerrorMessage(ftsInfo->conn));
		return false;
	}

	if (PQnfields(lastResult) != Natts_fts_message_response ||
		PQntuples(lastResult) != FTS_MESSAGE_RESPONSE_NTUPLES)
	{
		int			ntuples = PQntuples(lastResult);
		int			nfields = PQnfields(lastResult);

		PQclear(lastResult);
		elog(LOG, "FTS: invalid probe response from (content=%d, dbid=%d):"
			 "Expected %d tuple with %d field, got %d tuples with %d fields",
			 ftsInfo->segment_db_info->segindex, ftsInfo->segment_db_info->dbid,
			 FTS_MESSAGE_RESPONSE_NTUPLES, Natts_fts_message_response, ntuples, nfields);
		return false;
	}

	/*
	 * FTS_MSG_SYNCREP_OFF and FTS_MSG_PROMOTE response only needs an ack for
	 * now. In future iterations, we could parse the FTS_MSG_SYNCREP_OFF
	 * response to detect the case when mirror has come back up in-sync when
	 * previously thought not in-sync from probe response. In that situation,
	 * we should force another probe to update the gp_segment_configuration to
	 * avoid waiting the fts probe interval.
	 */
	if (ftsInfo->state == FTS_PROBE_SEGMENT)
	{
		probeRecordResponse(ftsInfo, lastResult);
		if (ftsInfo->result.retryRequested)
		{
			elog(LOG, "FTS: primary asked to retry the probe for (content=%d, dbid=%d)",
				 ftsInfo->segment_db_info->segindex, ftsInfo->segment_db_info->dbid);
			return false;
		}
	}

	return true;
}

void
FtsWalRepMessageSegments(fts_context *context)
{
	Assert(context);
}

/*
 * Check if probe timeout has expired
 */
static bool
probeTimeout(probe_response_per_segment *ftsInfo, const char* calledFrom)
{
	uint64 elapsed_ms = gp_get_elapsed_ms(&ftsInfo->startTime);
	const uint64 debug_elapsed_ms = 5000;

	if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE && elapsed_ms > debug_elapsed_ms)
	{
		elog(LOG, "FTS: probe '%s' elapsed time: " UINT64_FORMAT " ms for segment (content=%d, dbid=%d).",
			 calledFrom, elapsed_ms, ftsInfo->segment_db_info->segindex, ftsInfo->segment_db_info->dbid);
	}

	/* If connection takes more than the gp_fts_probe_timeout, we fail. */
	if (elapsed_ms > (uint64)gp_fts_probe_timeout * 1000)
	{
		elog(LOG, "FTS: failed to probe segment (content=%d, dbid=%d) due to timeout expiration, "
			 "probe elapsed time: " UINT64_FORMAT " ms.", ftsInfo->segment_db_info->segindex,
			 ftsInfo->segment_db_info->dbid, elapsed_ms);

		return true;
	}

	return false;
}

/*
 * Wait for a socket to become available for reading.
 */
static bool
probePollIn(probe_response_per_segment *ftsInfo)
{
	struct pollfd nfd;
	nfd.fd = PQsocket(ftsInfo->conn);
	nfd.events = POLLIN;
	int ret;

	do
	{
		ret = poll(&nfd, 1, 1000);

		if (ret > 0)
		{
			return true;
		}

		if (ret == -1 && errno != EAGAIN && errno != EINTR)
		{
			ftsInfo->probe_errno = errno;
			elog(LOG, "FTS: pollin error on libpq socket (content=%d, dbid=%d): %m",
				 ftsInfo->segment_db_info->segindex, ftsInfo->segment_db_info->dbid);

			return false;
		}
	} while (!probeTimeout(ftsInfo, "probePollIn"));

	elog(LOG, "FTS: pollin timeout waiting for libpq socket to become "
		 "available (content=%d, dbid=%d)", ftsInfo->segment_db_info->segindex,
		 ftsInfo->segment_db_info->dbid);

	return false;
}

/* EOF */
