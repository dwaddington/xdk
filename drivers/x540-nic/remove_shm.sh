#! /bin/bash

for i in $(sudo ipcs -m | grep "0x"| awk '{ print $1 }');
do
	echo "ipcrm -M $i"
	sudo ipcrm -M $i
done
