#!/bin/bash

set -e

make

for name in "basic" "relay"; do
    bin/ratesim json/${name}.json
    parser/run.py ${name}.log.gz -p ${name}.png -s 30
done
