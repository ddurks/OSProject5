#!/bin/sh

for ALGO in rand fifo custom
do
    for PROGRAM in scan sort focus
    do
        for FRAMES in 5 10 15 20 25 31 35 40 45 50 55 60 65 70 75 80 85 90 95 100
        do
            ./virtmem 100 $FRAMES $ALGO $PROGRAM >> results.csv
        done
    done
done
