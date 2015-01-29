#!/bin/bash
sudo watch "cat /proc/interrupts | /bin/grep pk0 | tr -s ' ' | cut -d ' ' -f 1-10"
