#!/bin/bash

HOME="/home/sergey/clavis/"
export HOME

gcc -g -lnuma -lm -lutil -pthread /home/sergey/clavis/scheduler.h /home/sergey/clavis/signal-handling.c /home/sergey/clavis/scheduler-tools.c /home/sergey/clavis/scheduler-algorithms.c -o scheduler.out

modprobe msr

killall perf
killall top
killall iotop
#nethogs -d 2 -o /mnt/kvm_images/sergey_vms/upload/clavis/nethogs.log br1 > /dev/null 2>&1 &
./scheduler.out d perf Xeon_X5570_woHT runfile.txt st once fixed 0
