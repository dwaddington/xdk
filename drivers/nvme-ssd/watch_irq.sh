#!/bin/bash

sudo watch "cat /proc/interrupts | /bin/grep pk | tr -s ' ' | cut -d ' ' -f 1-14 --output-delimiter='    ' "
#sudo watch 'cat /proc/interrupts | /bin/grep pk0 | sed 's/\s\s*/ /g\' | cut -d \' \' -f 1-10'
