#!/bin/bash

USE_SANITIZERS=0
SAN_BUILDIR=buildsan

sudo apt install -y ccache

configure_build_normal(){
cmake \
    -B build \
    -G Ninja \
    -DDEBUG=1 \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local/usr \
    -DCMAKE_INSTALL_SYSCONFDIR=$HOME/.local/etc \
    -DBUILD_TESTS=1 \
    -DBUILD_EXAMPLES=1 \
    -DUSE_SANITIZERS=0 \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
}

configure_build_san(){
cmake \
    -B $SAN_BUILDIR \
    -G Ninja \
    -DDEBUG=1 \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local/usr \
    -DCMAKE_INSTALL_SYSCONFDIR=$HOME/.local/etc \
    -DBUILD_TESTS=1 \
    -DBUILD_EXAMPLES=1 \
    -DUSE_SANITIZERS=1 \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
}

do_build(){
    ninja -C build install
}

do_build_san(){
    ninja -C $SAN_BUILDIR install
}

usage(){
cat <<EOF
 $0 [sanit]
   -h,--help,?                   show this help message and exit

If 'sanit' is specified as the positional argument, then an additional
build directory is created ('./$SAN_BUILDIR') where the program is linked
against sanitizers, and a build is performed there.
EOF
exit 0
}

parse_cli_args(){
    while [[ $# -gt 0 ]]; do
        local arg="$1"
        echo "current arg is '$arg'"
        shift

        case $arg in
          -h|--help|?)
              usage
              ;;
           sanit)
              USE_SANITIZERS=1
	          shift
	          ;;
           *)
             echo "unknown option: '$arg'"
             exit 1
        esac
    done
}

parse_cli_args $@

configure_build_normal
do_build

if [[ $USE_SANITIZERS -gt 0 ]]; then 
    configure_build_san
    do_build_san
fi

