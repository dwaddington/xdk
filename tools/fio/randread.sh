#!/bin/sh

sudo fio --filename=/dev/nvme0n1 --ioengine=libaio --direct=1 --norandommap --randrepeat=0 --runtime=600 --blocksize=4K --rw=randread --iodepth=32 --numjobs=8 --group_reporting --name=myjob
