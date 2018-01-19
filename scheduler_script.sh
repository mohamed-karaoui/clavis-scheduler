#!/bin/bash

RUNFILE=runfile.txt
REPORT_DIR="$HOME/.clavis"

mkdir -p $REPORT_DIR

gcc -DREPORTS_DIRECTORY="\"$REPORT_DIR\"" -g scheduler.h signal-handling.c scheduler-tools.c scheduler-algorithms.c -o scheduler.out -lnuma -lm -lutil -pthread 

modprobe msr

killall perf
killall top
killall iotop
#nethogs -d 2 -o /mnt/kvm_images/sergey_vms/upload/clavis/nethogs.log br1 > /dev/null 2>&1 &
./scheduler.out d perf Xeon_X5570_woHT runfile.txt st once fixed 0
