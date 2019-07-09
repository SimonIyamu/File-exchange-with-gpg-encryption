#!/bin/bash

# Collect arguments
if [ "$#" != 4 ]; then
    echo Error: Expected 4 arguments
    exit 2
fi

dir_name=$1
num_of_files=$2
num_of_dirs=$3
levels=$4

if [ "$num_of_dirs" -lt "$levels" ]; then
    echo Error: Number of directories must be bigger than levels
    exit 3
fi

if [ "$levels" == 0 -a "$num_of_dirs" != 0 ]; then
    echo Error: 0 levels can have 0 dirs.
    exit 4
fi

# If dir_name does not exist, create it
if [ ! -e "$dir_name" ]; then
    mkdir $dir_name
    echo Directory $dir_name was created.
fi

#--------------------------------
# Create directories

level=0
path=$dir_name
directories[0]="$dir_name"
for i in `seq 1 $num_of_dirs`; do
    level=`expr $level + 1`

    # Generate a random number between 1 and 8, for the name length
    let length="1 + RANDOM % 8"

    # Generate random string of that length, for the name of the new directory
    name=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w "$length" | head -n 1)

    # Go one level deeper
    path=$path/$name

    # Create this directory
    mkdir $path
    echo Directory $path was created.

    # Store in into the path array
    directories[$i]="$path"

    # If its deeper than levels, then restart from level 0 
    if [ "$level" == "$levels" ]; then
        level=0
        path=$dir_name
    fi
done

#--------------------------------
# Create files

for i in `seq 1 $num_of_files`; do

    # Generate a random number between 1 and 8, for the name length
    let length="1 + RANDOM % 8"

    # Generate random string of that length, for the name of the new directory
    name=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w "$length" | head -n 1)

    # Create the file path
    let index="($i - 1) % ($num_of_dirs + 1)"
    file_path="${directories[index]}/$name"

    # Generate a random number between 1Kb and 128Kb, for the file size
    let file_size="(1 + RANDOM % 128) * 1024"

    # Generate random string of that length, for the name of the new directory
    echo $(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w "$file_size" | head -n 1) >> $file_path
    echo File $file_path of size $file_size was created.
done
