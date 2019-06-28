-- start_ignore
CREATE LANGUAGE plpythonu;
-- end_ignore

CREATE OR REPLACE FUNCTION stat_db_objects(datname text, spcname text)
    RETURNS TABLE (dbid int2, relfilenode_dboid_relative_path text, size int)
    VOLATILE LANGUAGE plpythonu
AS
$fn$
import os
db_instances = {}
PG_DEFAULT_TSOID = 1663
PG_GLOBAL_TSOID = 1664

result = plpy.execute("select gp_tablespace_version_directory_name() as tablespace_version_dir_name;")
tablespace_version_dir_name = result[0]['tablespace_version_dir_name']

result = plpy.execute("SELECT oid AS dboid FROM pg_database WHERE datname='%s'" % datname)
dboid = result[0]['dboid']

result = plpy.execute("SELECT oid AS tsoid FROM pg_tablespace WHERE spcname='%s'" % spcname)
tsoid = result[0]['tsoid']

result = plpy.execute("select dbid, datadir from gp_segment_configuration;")
for col in range(0, result.nrows()):
    db_instances[result[col]['dbid']] = result[col]['datadir']

rows = []
for dbid, datadir in db_instances.items():
    relative_path_to_dboid_dir = ''
    if tsoid == PG_DEFAULT_TSOID:
        absolute_path_to_dboid_dir = '%s/base/%d' % (datadir, dboid)
    elif tsoid == PG_GLOBAL_TSOID:
        plpy.error("You can't have a database within the global tablespace")
    else:
        absolute_path_to_dboid_dir = '%(datadir)s/pg_tblspc/%(tsoid)d/%(tablespace_version_dir_name)s/%(dboid)d' % {
            'datadir': datadir,
            'tsoid': tsoid,
            'tablespace_version_dir_name': tablespace_version_dir_name,
            'dboid': dboid
        }

    try:
        for relfilenode in os.listdir(absolute_path_to_dboid_dir):
            relfilenode_absolute_path = absolute_path_to_dboid_dir + '/' + relfilenode
            size_relfilenode = os.stat(relfilenode_absolute_path).st_size
            row = {
                'relfilenode_dboid_relative_path': '%d/%s' % (dboid, relfilenode),
                'dbid': dbid,
                'size': size_relfilenode
            }

            rows.append(row)
    except OSError:
        plpy.notice("dboid dir for database %s does not exist on dbid = %d" % (datname, dbid))
        rows.append({
            'relfilenode_dboid_relative_path': None,
            'dbid': dbid,
            'size': None
        })

return rows
$fn$;