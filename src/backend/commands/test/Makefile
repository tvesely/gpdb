subdir=src/backend/commands
top_builddir=../../../..
include $(top_builddir)/src/Makefile.global

TARGETS=tablecmds vacuum

include $(top_builddir)/src/backend/mock.mk

tablecmds.t: \
	$(MOCK_DIR)/backend/access/aocs/aocs_compaction_mock.o

vacuum_test.o: FORCE

.PHONY: FORCE

test_vacuum: vacuum.t
	./vacuum.t

FORCE:
