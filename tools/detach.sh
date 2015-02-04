#!/bin/bash
#
# Utility to unbind a device from the parasitic kernel. Must be run as sudo or root.
#
full_pci=`cat /sys/class/parasite/$1/pci`
pci_id=`basename $full_pci`

echo $1 > /sys/class/parasite/detach

# unbind from PCI subsystem
#
echo $pci_id > /sys/bus/pci/drivers/parasite/unbind


