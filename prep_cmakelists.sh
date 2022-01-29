#!/bin/sh
src="CMakeLists.${1}"
test -f "$src" || { echo "usage: $0 [flash|sdcard]" >& 2; exit 2; }
cp "$src" CMakeLists.txt
