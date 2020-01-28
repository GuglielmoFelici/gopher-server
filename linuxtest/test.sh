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

compFile="compare_res"
outDir="out"
excludes=".*test/?.*|^./$|^../$"
echo "Testing files in $path
Recursion limited to $depth
Port $port"

if [[ $(curl gopher://localhost:$port// 2>&1) =~ '[Ff]ail' ]]; then
    echo "Failed to connect to server"
    exit
fi

test -f $compFile || touch $compFile
echo "" > $compFile
test -d $outDir || mkdir $outDir >/dev/null 2>&1
test "$(ls -A $outDir)" && rm -rf $outDir/* $outDir/.* 2>/dev/null 

result=0

for filename in $(find $path -maxdepth $depth); do
    if [[ $filename =~ $excludes ]]; then continue; fi
    echo $filename
    test -d $filename
    isDir=$?
    filename=${filename#$path}
    filename=${filename#/}
    outFile=$outDir/${filename//\//_}
    curl gopher://localhost:$port//${filename#$path} --output $outFile >/dev/null 2>&1
    if test $isDir -ne 0; then
        comp=$(cmp -l $path/$filename $outFile 2>&1)
        echo Comparing $path/$filename and $outFile... >> $compFile
        echo $comp >> $compFile
        if [[ $comp =~ ([0-9 ]{3} ?){3} ]]; then # tre gruppi di 1-3 cifre eventualmente separati da uno spazio
            result=1
        fi
    fi
done

test $result -eq 0 && echo "All tests passed successfully!"
test $result -eq 0 || echo "Some test failed"
echo "Read $compFile for comparison info."