#!/bin/bash

VERSION=3.10.8

set -x
unset MAKEFLAGS

mkdir -p dist
if [ ! -r dist/Python-${VERSION}.tgz ]; then
  curl https://www.python.org/ftp/python/${VERSION}/Python-${VERSION}.tgz --output dist/Python-${VERSION}.tgz
fi

if [ ! -d "$1" ] || [ ! -r "$1/lib/libpython3.10.so" ]; then
  echo building in $1
  rm -rf build/Python-${VERSION}

  tar -C build -zxf dist/Python-${VERSION}.tgz
  (
    cd build/Python-${VERSION}
    case `uname` in
      'Linux')
        export LDFLAGS="-Wl,-z,origin -Wl,-rpath,'\$\$ORIGIN/../lib'"
        ;;
      'Darwin')
        export LDFLAGS="-Wl,-rpath,@loader_path/../lib"
        export SSL="--with-openssl=$(brew --prefix openssl@1.1)"
        ;;
      *)
        echo 'Unsupported platform'
        exit 1
        ;;
    esac
    ./configure --prefix $1 --enable-shared --enable-optimizations --disable-test-modules ${SSL}
    make -j4 build_all
    make install
  )
  rm -f $1/python
  ln -s bin/python3 $1/python
fi
