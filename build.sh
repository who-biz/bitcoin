#!/bin/bash
# MIL build script for Ubuntu & Debian 9 v.3 (c) Decker (and webworker)
berkeleydb () {
    MIL_ROOT=$(pwd)
    MIL_PREFIX="${MIL_ROOT}/db4"
    mkdir -p $MIL_PREFIX
    wget -N 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
    echo '12edc0df75bf9abd7f82f821795bcee50f42cb2e5f76a6a281b85732798364ef db-4.8.30.NC.tar.gz' | sha256sum -c
    tar -xzvf db-4.8.30.NC.tar.gz
    cd db-4.8.30.NC/build_unix/

    ../dist/configure -enable-cxx -disable-shared -with-pic -prefix=$MIL_PREFIX

    make install
    cd $MIL_ROOT
}

buildmil () {
    git pull
    make clean
    ./autogen.sh
    ./configure LDFLAGS="-L${MIL_PREFIX}/lib/" CPPFLAGS="-I${MIL_PREFIX}/include/" --with-gui=no --disable-tests --disable-bench --without-miniupnpc --enable-experimental-asm --enable-static --disable-shared
    make -j$(nproc)
}

berkeleydb
buildmil
echo "Done building MIL!"
