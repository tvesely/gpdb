#!/bin/bash
set -euxo pipefail

SRC_DIR=gpdb_src

# the image should already have all the necessary apt-get packages for building .deb's

# cannot use 'mv' because of concourse inputs
# mv: inter-device move failed: 'debian_release/debian' to 'gpdb_src/debian'; unable to remove target: Directory not empty
cp -R debian_release/debian ${SRC_DIR}/

# Regex to capture required gporca version and download gporca source
ORCA_TAG=$(grep -Po 'v\d+.\d+.\d+' ${SRC_DIR}/depends/conanfile_orca.txt)
git clone --branch ${ORCA_TAG} https://github.com/greenplum-db/gporca.git ${SRC_DIR}/gporca

report_error() {
    local result=$1

    echo "***********************"
    echo "***********************"
    echo "Failed debuild; log is:"
    cat debuild.log

    exit ${result}
}

pushd ${SRC_DIR}
    VERSION=`./getversion | tr " " "."`-oss

    # EXTRA_CONFIGURE_FLAGS is used by debuild; see the debian-release repo
    # TODO: remove the EXTRA_CONFIGURE_FLAGS once zstd is added to the ubuntu docker images
    export EXTRA_CONFIGURE_FLAGS="--without-zstd"

    SHA=`git rev-parse --short HEAD`
    MESSAGE="Bumping to Greenplum version $VERSION, git SHA $SHA"
    PACKAGE=`cat debian/control | egrep "^Package: " | cut -d " " -f 2`

    dch --create --package $PACKAGE -v $VERSION "$MESSAGE"
    DEB_BUILD_OPTIONS='nocheck parallel=6' debuild -us -uc -b > debuild.log || report_error $?
popd

cp greenplum-db*.deb deb_package_ubuntu16/greenplum-db.deb
