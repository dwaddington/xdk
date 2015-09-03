#!/bin/bash
USER_ID=`id -u`
echo using UID = $USER_ID
sudo ~/xdk/tools/devgrant/devgrant --pciaddr 04:00.0 --uid $USER_ID

