---------------------------------------------------------------------
-- EXCHANGE PARTITION TESTS
---------------------------------------------------------------------
-- exchange partition should fail with dangling constraint ONLY on old table
---------------------------------------------------------------------
SELECT recreate_two_level_table();
DROP TABLE IF EXISTS e;
CREATE TABLE e (LIKE r INCLUDING CONSTRAINTS INCLUDING INDEXES);
ALTER TABLE r_1_prt_r2 ADD UNIQUE(r_key); -- add dangling constraint to parttion table
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
ALTER TABLE r EXCHANGE PARTITION r2 WITH TABLE e; -- should fail
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();

---------------------------------------------------------------------
-- exchange partition should fail with dangling constraint ONLY on new table
---------------------------------------------------------------------
SELECT recreate_two_level_table();
DROP TABLE IF EXISTS e;
CREATE TABLE e (LIKE r INCLUDING CONSTRAINTS INCLUDING INDEXES);
ALTER TABLE e ADD UNIQUE(r_key); -- add dangling constraint to new table
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
ALTER TABLE r EXCHANGE PARTITION r2 WITH TABLE e; -- should fail
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();


---------------------------------------------------------------------
-- exchange partition should work with dangling constraint on old and new table
---------------------------------------------------------------------
SELECT recreate_two_level_table();
DROP TABLE IF EXISTS e;
CREATE TABLE e (LIKE r INCLUDING CONSTRAINTS INCLUDING INDEXES);
ALTER TABLE r_1_prt_r2 ADD UNIQUE(r_key); -- add dangling constraint to parttion table
ALTER TABLE e ADD UNIQUE(r_key); -- add dangling constraint to new table
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
ALTER TABLE r EXCHANGE PARTITION r2 WITH TABLE e;
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();

---------------------------------------------------------------------
-- exchange partition: constraint on root should be added to old partition before exchange
---------------------------------------------------------------------
SELECT recreate_two_level_table();
DROP TABLE IF EXISTS e;
CREATE TABLE e (LIKE r INCLUDING CONSTRAINTS INCLUDING INDEXES);
SET sql_inheritance TO off;
ALTER TABLE r ADD UNIQUE(r_key); -- add dangling constraint to new table
SET sql_inheritance TO on;
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
ALTER TABLE r EXCHANGE PARTITION r2 WITH TABLE e; -- should fail
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();



---------------------------------------------------------------------
-- exchange partition: constraint on intermediate partition should be added to new partition before exchange
---------------------------------------------------------------------
DROP TABLE IF EXISTS r;
CREATE TABLE r (r_key CHAR(25), r_int INTEGER NOT NULL)
  DISTRIBUTED BY (r_key) PARTITION BY LIST (r_key)
  SUBPARTITION BY range (r_int) SUBPARTITION TEMPLATE
(SUBPARTITION r1 START(1) END(3))
(PARTITION a VALUES ('A'), PARTITION b VALUES ('B'));
DROP TABLE IF EXISTS e2;
CREATE TABLE e2 (LIKE r INCLUDING CONSTRAINTS INCLUDING INDEXES);
set sql_inheritance to off;
ALTER TABLE r_1_prt_a ADD UNIQUE(r_key,r_int); -- add dangling constraint to parttion table
set sql_inheritance to on;
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
ALTER TABLE r EXCHANGE PARTITION a with table e;
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();

-------------------------------------------------------------------------------
-- SPLIT PARTITION TESTS
---------------------------------------------------------------------
-- split partition should fail with dangling constraint on leaf table
---------------------------------------------------------------------

SELECT recreate_two_level_table();
ALTER TABLE r_1_prt_r2 ADD UNIQUE(r_key); -- add dangling constraint to parttion table
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
ALTER TABLE r SPLIT PARTITION r2 AT (6) INTO (PARTITION r2_split_l, PARTITION r2_split_r);  --should not work FIXME: reword error message
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();

---------------------------------------------------------------------
-- split partition fail with dangling constraint on root partition, but not leaf
---------------------------------------------------------------------
SELECT recreate_two_level_table();
set sql_inheritance to off;
ALTER TABLE r ADD UNIQUE(r_key); -- add dangling constraint to parttion table
set sql_inheritance to on;
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
ALTER TABLE r SPLIT PARTITION r2 AT (6) INTO (PARTITION r2_split_l, PARTITION r2_split_r);  --should not work FIXME: reword error message
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();

---------------------------------------------------------------------
-- split partition should fail with dangling constraint on intermediate partition, but not leaf
---------------------------------------------------------------------
DROP TABLE IF EXISTS r;
CREATE TABLE r (r_key CHAR(25), r_int INTEGER NOT NULL)
  DISTRIBUTED BY (r_key) PARTITION BY LIST (r_key)
  SUBPARTITION BY range (r_int) SUBPARTITION TEMPLATE
(SUBPARTITION r1 START(1) END(3))
(PARTITION a VALUES ('A'), PARTITION b VALUES ('B'));
set sql_inheritance to off;
ALTER TABLE r_1_prt_r1 ADD UNIQUE(r_key,rint); -- add dangling constraint to parttion table
set sql_inheritance to on;
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
ALTER TABLE r SPLIT PARTITION r_1_prt_a1_2_prt_r1 AT (6) INTO (PARTITION r1_split_l, PARTITION r1_split_r);  --should not work FIXME: reword error message
SELECT partition_name, parent_name, root_name, constraint_name, index_name, constraint_type FROM partition_tables_show_all();
