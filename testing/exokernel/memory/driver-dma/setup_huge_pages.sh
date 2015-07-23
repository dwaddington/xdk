#! /bin/bash

sudo echo 8 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

cat /proc/meminfo
