#!/bin/sh
function format()
{
        printf "%-10s%8d%8d\n" $1 $2 $3
}

function statistics()
{
        FILES=$(find . -name "$1"|wc -l)
        LINES=$(cat $(find . -name "$1")|wc -l|awk '{print $1;}')
        format "$2" $FILES $LINES
        TOTAL_FILES=$(expr $TOTAL_FILES + $FILES)
        TOTAL_LINES=$(expr $TOTAL_LINES + $LINES)
}

statistics '*.h'  "header"
statistics '*.S'  "assembly"
statistics '*.cc' "cplusplus"
format "total" $TOTAL_FILES $TOTAL_LINES
