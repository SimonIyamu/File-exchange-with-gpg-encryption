#!/bin/bash

i=0
sBytes=0
rBytes=0
sFiles=0
rFiles=0
left=0
while read id mode filename bytes; do
    # Check if id exists already in the idList
    exists=0
    for item in `echo ${idList[*]}`; do
        if [ "$item" == "$id" ]; then
            exists=1
            break
        fi
    done
  
    # If it doest exist in the list, add it to the list
    if [ "$exists" == 0 ]; then
        # P.S. im not a $exist

        # Store the id to the list
        idList[i]=$id
        let i="$i+1"
    fi

    if [ "$mode" == 's' ]; then
        let sFiles="$sFiles + 1"
        let sBytes="$sBytes + $bytes"
    fi
    if [ "$mode" == 'r' ]; then
        let rFiles="$rFiles + 1"
        let rBytes="$rBytes + $bytes"
    fi
    if [ "$mode" == 'x' ]; then
        let left="$left + 1"
    fi
done

echo There are $i clients: ${idList[*]}

# Find min and max in the idList
max=${idList[0]}
min=${idList[0]}
for id in `echo ${idList[*]}`; do
    if [ "$id" -gt "$max" ]; then
        max=$id
    fi
    if [ "$id" -lt "$min" ]; then
        min=$id
    fi
done

echo Max id: $max
echo Min id: $min
echo Sent bytes: $sBytes
echo Received bytes: $rBytes
echo Sent files: $sFiles
echo Received files: $rFiles
echo Ammount of clients that left: $left

