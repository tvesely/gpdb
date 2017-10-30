#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

#define Assert(condition) if (!condition) AssertFailed()

bool is_assert_failed = false;

void AssertFailed()
{
	is_assert_failed = true;
}


/* Actual function body */
#include "../fts.c"

static CdbComponentDatabases *
makeTestCdb(int segCnt, bool has_mirrors, bool has_standby)
{
	int			i = 0;
	int 		mirror_multiplier = 1;
	int			entryCnt = 1;

	if (has_mirrors)
		mirror_multiplier = 2;

	if (has_standby)
		entryCnt = 2;

	CdbComponentDatabases *cdb = (CdbComponentDatabases *) malloc(sizeof(CdbComponentDatabases));
	cdb->total_entry_dbs = entryCnt;
	cdb->total_segment_dbs = segCnt * mirror_multiplier;	/* with mirror? */
	cdb->total_segments = cdb->total_segment_dbs + cdb->total_entry_dbs;
	cdb->my_dbid = 1;
	cdb->my_segindex = -1;
	cdb->my_isprimary = true;
	cdb->entry_db_info = malloc(
			sizeof(CdbComponentDatabaseInfo) * cdb->total_entry_dbs);
	cdb->segment_db_info = malloc(
			sizeof(CdbComponentDatabaseInfo) * cdb->total_segment_dbs);

	for (i = 0; i < cdb->total_entry_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb->entry_db_info[i];

		cdbinfo->dbid = 1;
		cdbinfo->segindex = '-1';

		cdbinfo->role = 'p';
		cdbinfo->preferred_role = 'p';
		cdbinfo->status = 'u';
	}

	for (i = 0; i < cdb->total_segment_dbs; i++) {
		CdbComponentDatabaseInfo *cdbinfo = &cdb->segment_db_info[i];


		cdbinfo->dbid = i + 2;
		cdbinfo->segindex = i / 2;

		if (has_mirrors) {
			cdbinfo->role = i % 2 ? 'm' : 'p';
			cdbinfo->preferred_role = i % 2 ? 'm' : 'p';
		} else {
			cdbinfo->role = 'p';
			cdbinfo->preferred_role = 'p';
		}
		cdbinfo->status = 'u';

	}

	return cdb;
}

void
set_segment_status(int16 dbid, char state)
{
	if(dbid <= 1 || dbid > cdb_component_dbs->total_segments)
		return;

	cdb_component_dbs->segment_db_info[dbid - 2].status = state;
}

void
test_probeWalRepPublishUpdate(void **state)
{
	probe_context context;
	cdb_component_dbs = makeTestCdb(3, true, true);

	set_segment_status(2, 'd');
	set_segment_status(3, 'd');
	//set_segment_status(5, 'd');

	context.responses = malloc(sizeof(probe_response_per_segment));
	context.count = 1;

	context.responses->segment_db_info = FtsGetPeerSegment(0,3);
	context.responses->result.dbid = 3;
	context.responses->result.isPrimaryAlive = true;
	context.responses->result.isMirrorAlive = false;

	expect_value(probeWalRepUpdateConfig, dbid, 3);
	expect_value(probeWalRepUpdateConfig, segindex, 0);
	expect_value(probeWalRepUpdateConfig, IsSegmentAlive, true);
	will_be_called(probeWalRepUpdateConfig);

	probeWalRepPublishUpdate(&context);





	/*
	bool expected_mirror_state = true;

	will_return(IsMirrorUp, expected_mirror_state);

	expect_any(initStringInfo, str);
	will_be_called(initStringInfo);

	expect_any(pq_beginmessage, buf);
	expect_value(pq_beginmessage, msgtype, '\0');
	will_be_called(pq_beginmessage);

	ProbeResponse response;
	response.IsMirrorUp = expected_mirror_state;

	expect_any(pq_sendbytes, buf);
	expect_memory(pq_sendbytes, data, &response, sizeof(response));
	expect_value(pq_sendbytes, datalen, sizeof(ProbeResponse));
	will_be_called(pq_sendbytes);

	expect_any(pq_endmessage, buf);
	will_be_called(pq_endmessage);

	will_be_called(pq_flush);

*/
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_probeWalRepPublishUpdate)
	};
	return run_tests(tests);
}
