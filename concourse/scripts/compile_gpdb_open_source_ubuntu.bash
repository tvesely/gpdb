#!/bin/bash -l
set -exo pipefail

GREENPLUM_INSTALL_DIR=/usr/local/gpdb
TRANSFER_DIR_ABSOLUTE_PATH=$(pwd)/${TRANSFER_DIR}
COMPILED_BITS_FILENAME=${COMPILED_BITS_FILENAME:="compiled_bits_ubuntu16.tar.gz"}

function build_external_depends() {
    # fix the /root/.ccache missing issue during build
    mkdir -p /root/.ccache && touch  /root/.ccache/ccache.conf
    pushd gpdb_src/depends
        ./configure
        if ! make > build_external_depends.log 2>&1 ; then
            cat build_external_depends.log
            return 1
        fi
        grep -v "Installing:" build_external_depends.log
        make
    popd
}

function install_external_depends() {
    pushd gpdb_src/depends
        make install
    popd
}

function build_gpdb() {
    build_external_depends
    pushd gpdb_src
        # TODO: remove this flag after adding zstd to the ubuntu build images
        CONFIGURE_FLAGS="${CONFIGURE_FLAGS} --without-zstd"
        CWD=$(pwd)
        LD_LIBRARY_PATH=${CWD}/depends/build/lib CC=$(which gcc) CXX=$(which g++) ./configure --enable-mapreduce --enable-orafce --with-gssapi --with-perl --with-libxml \
          --with-python \
          --with-libraries=${CWD}/depends/build/lib \
          --with-includes=${CWD}/depends/build/include \
          --prefix=${GREENPLUM_INSTALL_DIR} \
          ${CONFIGURE_FLAGS}
        make -j4 -s
        LD_LIBRARY_PATH=${CWD}/depends/build/lib make install
    popd
    install_external_depends
}

function unittest_check_gpdb() {
  make -C gpdb_src/src/backend -s unittest-check
}

function export_gpdb() {
  TARBALL="$TRANSFER_DIR_ABSOLUTE_PATH"/$COMPILED_BITS_FILENAME
  pushd $GREENPLUM_INSTALL_DIR
    source greenplum_path.sh
    python -m compileall -q -x test .
    chmod -R 755 .
    tar -czf ${TARBALL} ./*
  popd
}

function _main() {
    build_gpdb
    unittest_check_gpdb
    export_gpdb
}

_main "$@"
