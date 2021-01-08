#!/bin/bash -x

PID=$( ps aux|grep '^readsb'|awk '{print $2}' )
sudo taskset -a -p -c 0,1 $PID
