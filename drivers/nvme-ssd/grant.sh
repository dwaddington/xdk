#!/bin/bash
USER_ID=`id -u`
echo using UID = $USER_ID
sudo ~/xdk/tools/devgrant/devgrant --pciaddr 06:00.0 --uid $USER_ID
sudo ~/xdk/tools/devgrant/devgrant --pciaddr 47:00.0 --uid $USER_ID
