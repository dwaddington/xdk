#!/bin/sh

sudo fio --filename=/dev/nvme0n1 --rw=write --ioengine=libaio --direct=1 --blocksize=128K --size=370G --iodepth=32 --group_reporting --name=myjob
