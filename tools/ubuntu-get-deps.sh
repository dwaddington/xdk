#!/bin/bash
PACKAGES='make g++ libnuma-dev libtinyxml-dev libboost-dev libboost-thread-dev '
PACKAGES+='libboost-program-options-dev libboost-filesystem-dev'
sudo apt-get install $PACKAGES
