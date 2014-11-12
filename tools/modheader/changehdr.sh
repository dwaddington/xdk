#!/bin/bash
MODHEADER_BIN=$HOME/xdk-seed/tools/modheader/modheader

if [ ! $1 ] ; then
  echo "changehdr <new_header_file>"
fi

HEADERS=`find . -name *.h`
CXXFILES=`find . -name *.cc`
CFILES=`find . -name *.c`

for i in $HEADERS; do
    echo "Modifying header in file $i ..."
    $MODHEADER_BIN $i $1
done;
for i in $CXXFILES ; do
    echo "Modifying header in file $i ..."
    $MODHEADER_BIN $i $1
done;
for i in $CFILES; do
    echo "Modifying header in file $i ..."
    $MODHEADER_BIN $i $1
done;
