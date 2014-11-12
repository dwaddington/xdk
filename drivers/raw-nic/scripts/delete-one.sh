#!/bin/bash

ip link set dev $1 down
ip link delete $1
