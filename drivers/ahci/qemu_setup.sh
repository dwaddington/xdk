#!/bin/bash
# set up for QEMU 
#
echo "Don't forget to load parasitic module!"
DEVGRANT=../../tools/devgrant/devgrant
sudo rmmod ahci
sudo $DEVGRANT --pciaddr 00:05.0 --uid 1000

