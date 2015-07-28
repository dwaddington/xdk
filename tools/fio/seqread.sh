#!/bin/sh

sudo fio --filename=/dev/nvme0n1 --rw=read --ioengine=libaio --direct=1 --blocksize=128K --runtime=300 --iodepth=32 --group_reporting --name=myjob
