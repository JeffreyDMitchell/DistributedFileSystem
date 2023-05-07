#!/bin/bash

# kill older servers if they exist
pkill dfs

# start new servers
for i in {1..4}; do
    ./dfs "./server${i}/" "1000${i}" &
done
