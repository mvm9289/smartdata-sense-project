#!/bin/bash

CONTIKI_DIR=../../contiki-2.x
LIB_FILES=(
    LibSensors/zt-sensor-lib.*
    LibPacketRIME/zt-packet-mgmt.*
    LibFilesystem/zt-filesys-lib.*
)

for lib_file in ${LIB_FILES[@]}
do
    cp $lib_file $CONTIKI_DIR/core/lib/
done

