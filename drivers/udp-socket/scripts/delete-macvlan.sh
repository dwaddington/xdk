#!/bin/bash

TESTHOST=www.google.com
MACVLAN_PREFIX=macvlan
MAC_ADDR_PREFIX=a0:36:9f:1a:57:f
IP_PREFIX=10.0.0.10


if [[ "$#" -ne 2 ]]; then
	echo "[USAGE] $0 <ethX> <number of macvlans>"
	exit
fi

HWLINK=$1
NUM_MACVLAN=$2

if [[ $NUM_MACVLAN -gt 16 ]];  then
	echo "ASSUME the number of macvlan's is less or equal to 16"
	exit
fi
# ------------
# wait for network availability
# ------------

while ! ping -q -c 1 $TESTHOST > /dev/null
do
	echo "$0: Cannot ping $TESTHOST, waiting another 5 secs..."
	sleep 5
done

#IP=$(ip address show dev $HWLINK | `which grep` "inet " | `which awk` '{print $2}')
#
#echo
#echo "$HWLINK's IP is $IP"
#echo "Use the same IP for all macvlan for now"
#echo

# ------------
# setting up $MACVLN interface
# ------------
for idx in `seq 1 $NUM_MACVLAN`
do 
	echo
	echo "===== Setting MAC VLAN #$idx ====="
	mac_idx=$[$idx-1]
	MACVLAN=${MACVLAN_PREFIX}${mac_idx}
	echo "Interface: $MACVLAN"
	MAC_ADDR=${MAC_ADDR_PREFIX}${mac_idx}
	echo "MAC addr : $MAC_ADDR"
	#IP=${IP_PREFIX}${mac_idx}
	IP=${IP_PREFIX}0
	echo "IP       : $IP"

#-------------
#	First delete existing interface
#-------------
	echo
	echo "** Delete exisiting interface"
	echo "ip link set dev $MACVLAN down"
	ip link set dev $MACVLAN down
	echo "ip link delete $MACVLAN"
	ip link delete $MACVLAN
	echo 

done
#ip link add link $HWLINK $MACVLN address $MAC_ADDR type macvlan
#ip address add $IP dev $MACVLN
#ip link set dev $MACVLN up

# ------------
# routing table
# ------------

# empty routes
#ip route flush dev $HWLINK
#ip route flush dev $MACVLN

# add routes
#ip route add $NETWORK dev $MACVLN metric 0

# add the default gateway
#ip route add default via $GATEWAY

