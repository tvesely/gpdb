CREATE EXTENSION IF NOT EXISTS gp_inject_fault;


\! ps -ef | grep dbfast_mirror1 | grep -v grep | awk '{print $2}' | xargs kill -9

-- select pg_sleep(90);

-- the mirror should be down now
select * from gp_segment_configuration;

BEGIN;
COMMIT;
select gp_inject_fault_new('dispatch_result_poll', 'skip', 1);

select gp_remove_segment_mirror(0::smallint);

--\! gprecoverseg -a

select * from gp_segment_configuration;
