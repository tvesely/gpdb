from behave import given, when, then

from test.behave_utils.utils import *

# This file contains steps for gpaddmirrors and gpmovemirrors tests

def _get_mirror_count():
    with dbconn.connect(dbconn.DbURL(dbname='template1')) as conn:
        sql = """SELECT count(*) FROM gp_segment_configuration WHERE role='m'"""
        count_row = dbconn.execSQL(conn, sql).fetchone()
        return count_row[0]


def _generate_input_config(spread=False):
    datadir_config = _write_datadir_config()

    mirror_config_output_file = "/tmp/test_gpaddmirrors.config"
    cmd_str = 'gpaddmirrors -a -o %s -m %s' % (mirror_config_output_file, datadir_config)
    if spread:
        cmd_str += " -s"
    Command('generate mirror_config file', cmd_str).run(validateAfter=True)

    return mirror_config_output_file


def do_write(template, config_file_path):
    mirror_data_dir = make_data_directory_called('mirror')

    with open(config_file_path, 'w') as fp:
        contents = template.format(mirror_data_dir)
        fp.write(contents)


def _write_datadir_config():
    datadir_config = '/tmp/gpaddmirrors_datadir_config'

    template = """
{0}
{0}
"""

    do_write(template, datadir_config)

    return datadir_config


def _write_datadir_config_for_three_mirrors():
    datadir_config='/tmp/gpaddmirrors_datadir_config'

    template = """
{0}
{0}
{0}
"""

    do_write(template, datadir_config)

    return datadir_config


def add_three_mirrors(context):
    datadir_config = _write_datadir_config_for_three_mirrors()
    mirror_config_output_file = "/tmp/test_gpaddmirrors.config"

    cmd_str = 'gpaddmirrors -o %s -m %s' % (mirror_config_output_file, datadir_config)
    Command('generate mirror_config file', cmd_str).run(validateAfter=True)

    cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s ' % mirror_config_output_file)
    cmd.run(validateAfter=True)


def add_mirrors(context):
    context.mirror_config = _generate_input_config()

    cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s ' % context.mirror_config)
    cmd.run(validateAfter=True)


def make_data_directory_called(data_directory_name):
    mdd_parent_parent = os.path.realpath(
        os.getenv("MASTER_DATA_DIRECTORY") + "../../../")
    mirror_data_dir = os.path.join(mdd_parent_parent, data_directory_name)
    if not os.path.exists(mirror_data_dir):
        os.mkdir(mirror_data_dir)
    return mirror_data_dir


@then('verify the database has mirrors')
def impl(context):
    if _get_mirror_count() == 0:
        raise Exception('No mirrors found')


@given('gpaddmirrors adds mirrors')
@when('gpaddmirrors adds mirrors')
@then('gpaddmirrors adds mirrors')
def impl(context):
    add_mirrors(context)


@given('gpaddmirrors adds mirrors with temporary data dir')
def impl(context):
    context.mirror_config = _generate_input_config()
    mdd = os.getenv('MASTER_DATA_DIRECTORY', "")
    del os.environ['MASTER_DATA_DIRECTORY']
    try:
        cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s -d %s' % (context.mirror_config, mdd))
        cmd.run(validateAfter=True)
    finally:
        os.environ['MASTER_DATA_DIRECTORY'] = mdd


@given('gpaddmirrors adds mirrors in spread configuration')
def impl(context):
    context.mirror_config = _generate_input_config(spread=True)

    cmd = Command('gpaddmirrors ', 'gpaddmirrors -a -i %s ' % context.mirror_config)
    cmd.run(validateAfter=True)


@then('save the gparray to context')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    context.gparray = gparray


@then('mirror hostlist matches the one saved in context')
def impl(context):
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    old_contentId_to_host = {}
    curr_contentId_to_host = {}

    # Map content IDs to hostnames for every mirror, for both the saved GpArray
    # and the current one.
    for (array, hostMap) in [(context.gparray, old_contentId_to_host), (gparray, curr_contentId_to_host)]:
        for host in array.get_hostlist(includeMaster=False):
            for mirror in array.get_list_of_mirror_segments_on_host(host):
                hostMap[mirror.getSegmentContentId()] = host

    if len(curr_contentId_to_host) != len(old_contentId_to_host):
        raise Exception("Number of mirrors doesn't match between old and new clusters")

    for key in old_contentId_to_host.keys():
        if curr_contentId_to_host[key] != old_contentId_to_host[key]:
            raise Exception("Mirror host doesn't match for contentId %s (old host=%s) (new host=%s)"
            % (key, old_contentId_to_host[key], curr_contentId_to_host[key]))


@then('verify the database has mirrors in spread configuration')
def impl(context):
    hostname_to_primary_contentId_list = {}
    mirror_contentId_to_hostname = {}
    gparray = GpArray.initFromCatalog(dbconn.DbURL())
    host_list = gparray.get_hostlist(includeMaster=False)

    for host in host_list:
        primaries = gparray.get_list_of_primary_segments_on_host(host)
        primary_content_list = []
        for primary in primaries:
            primary_content_list.append(primary.getSegmentContentId())

        hostname_to_primary_contentId_list[host] = primary_content_list

        mirrors = gparray.get_list_of_mirror_segments_on_host(host)
        for mirror in mirrors:
            mirror_contentId_to_hostname[mirror.getSegmentContentId()] = host

    # Verify that for each host its mirrors are correctly spread.
    # That is for a given host, all of its primaries would have
    # mirrors on separate hosts.  Therefore, we go through the list of
    # primaries on a host and compute the set of hosts which have the
    # corresponding mirrors.  If spreading is working, then the
    # cardinality of the mirror_host_set should equal the number of
    # primaries on the given host.

    for hostname in host_list:

        # For the primaries on a given host, put the hosts of the
        # corresponding mirrors into this set (a set to eliminate
        # duplicate hostnames in case two mirrors end up on the same
        # host).

        mirror_host_set = set()
        primary_content_list = hostname_to_primary_contentId_list[hostname]
        for contentId in primary_content_list:
            mirror_host = mirror_contentId_to_hostname[contentId]
            if mirror_host == hostname:
                raise Exception('host %s has both primary and mirror for contentID %d' %
                                (hostname, contentId))

            mirror_host_set.add(mirror_host)

        num_primaries = len(primary_content_list)
        num_mirror_hosts = len(mirror_host_set)
        if num_primaries != num_mirror_hosts:
            raise Exception('host %s has %d primaries spread on only %d hosts' %
                            (hostname, num_primaries, num_mirror_hosts))
