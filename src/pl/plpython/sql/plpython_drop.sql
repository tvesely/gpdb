--
-- For paranoia's sake, don't leave an untrusted language sitting around
--
SET client_min_messages = WARNING;

<<<<<<< HEAD
DROP TABLE users CASCADE; 
DROP TABLE taxonomy CASCADE; 
DROP TABLE sequences CASCADE; 
DROP TABLE xsequences CASCADE;  
DROP TABLE unicode_test CASCADE;  
DROP TABLE table_record CASCADE; 
DROP TYPE  type_record CASCADE; 
DROP TYPE  ab_tuple CASCADE; 

DROP PROCEDURAL LANGUAGE plpythonu CASCADE;
=======
DROP EXTENSION plpythonu CASCADE;

DROP EXTENSION IF EXISTS plpython2u CASCADE;
>>>>>>> a4bebdd92624e018108c2610fc3f2c1584b6c687
