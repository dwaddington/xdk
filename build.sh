#!/bin/bash

XDK_HOME=`pwd`

RED='\e[0;31m'
GREEN='\e[0;32m'
RESET='\e[0m'

width=50

function build_dir {
printf "%-$width.${width}s" "[BUILD] $1 ($2) ... "
cd ${XDK_HOME}/$3 ; make clean &>> ${XDK_HOME}/build.log; make  &>> ${XDK_HOME}/build.log
[ $? -eq 0 ] && echo -e " ${GREEN}OK${RESET}" || echo -e " ${RED}FAILED${RESET}"
}

build_dir "core" "Kernel Module" kernel/modules/parasite
build_dir "library" "libcommon" lib/libcommon
build_dir "library" "libcomponent" lib/libcomponent
build_dir "library" "libexo" lib/libexo
build_dir "driver" "AHCI device driver" drivers/ahci
build_dir "driver" "NVME-SSD device driver" drivers/nvme-ssd
build_dir "component" "Base example component" examples/components/base-example
build_dir "component" "Dummy block device component" examples/components/block-device

