#!/bin/sh

# quick'n'dirty tool to convert lld generated map into something that bochs understands
# add "-map:output.map" to the lld line in Makefile to get map


tail -n +15 output.map | while read l; do
    if [ "$l" == "" ]; then break; fi
    line=`echo "$l"|sed 's/\ \ /\ /g'`
    echo `echo $line|cut -d ' ' -f 3` `echo $line|cut -d ' ' -f 2|tr '?$' '__'`
done >output.sym

