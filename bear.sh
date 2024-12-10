#!/bin/bash
# shellcheck disable=SC2129
# shellcheck disable=SC2046

cd ./.vscode/ || exit
make clean -C ../lib/linux/
bear --append -- make all -C ../lib/linux/
bear --append -- make all -C ../apps/ep/linux/
bear --append -- make all -C ../apps/is/linux/
bear --append -- make all -C ../apps/lu/linux/
bear --append -- make all -C ../apps/mm/linux/
bear --append -- make all -C ../apps/pi/linux/
bear --append -- make all -C ../apps/sor/linux/
bear --append -- make all -C ../apps/tsp/linux/
bear --append -- make all -C ../apps/water/linux/
cd .. || exit