#!/bin/sh

OPTIND=1         # Reset in case getopts has been used previously in the shell.

port="7070"
path="./"
depth="2"

while getopts "d:p:r:" opt; do
    case "$opt" in
    h|\?)
        echo "Usage:
        test [OPTION]
        -d directory | -p port | -r recursion depth"
        exit 0
        ;;
    p)  port=$OPTARG
        ;;
    d)  path=$OPTARG
        ;;
    r)  depth=$OPTARG
    esac
done

compFile="compareRes"
outDir="out"
excludes=".*test/?.*|^./$|^../$"
echo "Testing files in $path
Recursion limited to $depth
Port $port"

test -f $compareRes || touch $compareRes
test -d $outDir || mkdir $outDir >/dev/null 2>&1
test "$(ls -A $outDir)" && rm -rf $outDir/* $outDir/.* 2>/dev/null 

for filename in $(find $path -maxdepth $depth); do
    if [[ $filename =~ $excludes ]]; then continue; fi
    test -d $filename
    isDir=$?
    filename=${filename#$path}
    filename=${filename#/}
    curl gopher://localhost:$port//${filename#$path}$(test $isDir -eq 0 && echo /) --output $outDir/${filename//\//_} >/dev/null 2>&1
    test $isDir -ne 0 && cmp 
done