#!/bin/sh

sudo fio --filename=/dev/nvme0n1 --ioengine=libaio --direct=1 --norandommap --randrepeat=0 --runtime=1800 --blocksize=4K --rw=randwrite --iodepth=32 --numjobs=8 --group_reporting --name=myjob
