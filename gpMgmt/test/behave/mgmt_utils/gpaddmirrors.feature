@gpaddmirrors
Feature: Tests for gpaddmirrors

    Scenario: gprecoverseg works correctly on a newly added mirror
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is killed on hosts "mdw,sdw1,sdw2"
        And a cluster is created with no mirrors on "mdw" and "sdw1, sdw2"
        And gpaddmirrors adds mirrors
        Then verify the database has mirrors
        And the information of a "mirror" segment on a remote host is saved
        When user kills a "mirror" process with the saved information
        When the user runs "gprecoverseg -a"
        Then gprecoverseg should return a return code of 0
        And all the segments are running
        And the segments are synchronized
        And the user runs "gpstop -aqM fast"

    Scenario: gpaddmirrors puts mirrors on the same hosts when there is a standby configured
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is killed on hosts "mdw,sdw1,sdw2,sdw3"
        And a cluster is created with no mirrors on "mdw" and "sdw1, sdw2, sdw3"
        And gpaddmirrors adds mirrors
        Then verify the database has mirrors
        And save the gparray to context
        And the database is killed on hosts "mdw,sdw1,sdw2,sdw3"
        And a cluster is created with no mirrors on "mdw" and "sdw1, sdw2, sdw3"
        And the user runs gpinitstandby with options " "
        Then gpinitstandby should return a return code of 0
        And gpaddmirrors adds mirrors
        Then mirror hostlist matches the one saved in context
        And the user runs "gpstop -aqM fast"

    # This test requires a bigger cluster than the other gpaddmirrors tests.
    @gpaddmirrors_spread
    Scenario: gpaddmirrors puts mirrors on different host
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is killed on hosts "mdw,sdw1,sdw2,sdw3"
        And a cluster is created with no mirrors on "mdw" and "sdw1, sdw2, sdw3"
        And gpaddmirrors adds mirrors in spread configuration
        Then verify that mirror segments are in "spread" configuration
        And the user runs "gpstop -aqM fast"

    Scenario: gpaddmirrors with a default master data directory
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And gpaddmirrors adds mirrors
        Then verify the database has mirrors
        And the user runs "gpstop -aqM fast"

    @gpaddmirrors_temp_directory
    Scenario: gpaddmirrors with a given master data directory [-d <master datadir>]
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And gpaddmirrors adds mirrors with temporary data dir
        Then verify the database has mirrors
        And the user runs "gpstop -aqM fast"

    @gpaddmirrors_mirrors_restart
    Scenario: gpaddmirrors mirrors are recognized after a cluster restart
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        When gpaddmirrors adds mirrors
        Then verify the database has mirrors
        When an FTS probe is triggered
        And the user runs "gpstop -a"
        And wait until the process "gpstop" goes down
        And the user runs "gpstart -a"
        And wait until the process "gpstart" goes down
        Then all the segments are running
        And the segments are synchronized
        And the user runs "gpstop -aqM fast"

    @gpaddmirrors_workload
    Scenario: gpaddmirrors when the primaries have data
        Given a working directory of the test as '/tmp/gpaddmirrors'
        And the database is killed on hosts "mdw,sdw1"
        And a cluster is created with no mirrors on "mdw" and "sdw1"
        And database "gptest" exists
        And there is a "heap" table "public.heap_table" in "gptest" with "100" rows
        And there is a "ao" table "public.ao_table" in "gptest" with "100" rows
        And there is a "co" table "public.co_table" in "gptest" with "100" rows
        And gpaddmirrors adds mirrors with temporary data dir
        And an FTS probe is triggered
        And the segments are synchronized
        When user kills all primary processes with SIGKILL
        And user can start transactions
        Then verify that there is a "heap" table "public.heap_table" in "gptest" with "100" rows
        Then verify that there is a "ao" table "public.ao_table" in "gptest" with "100" rows
        Then verify that there is a "co" table "public.co_table" in "gptest" with "100" rows
        And the user runs "gpstop -aqM fast"
