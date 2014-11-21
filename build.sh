#!/bin/bash

XDK_HOME=`pwd`

RED='\e[0;31m'
GREEN='\e[0;32m'
RESET='\e[0m'

width=50

function build_dir {
printf "%-$width.${width}s" "[BUILD] library ($1) ..."
cd ${XDK_HOME}/$2 ; make clean &>> ${XDK_HOME}/build.log; make  &>> ${XDK_HOME}/build.log
[ $? -eq 0 ] && echo -e "${GREEN}OK${RESET}" || echo -e "${RED}FAILED${RESET}"
}

build_dir "Kernel Module" kernel/modules/parasite
build_dir "libcommon" lib/libcommon
build_dir "libcomponent" lib/libcomponent
build_dir "libexo" lib/libexo
build_dir "AHCI device driver" drivers/ahci


