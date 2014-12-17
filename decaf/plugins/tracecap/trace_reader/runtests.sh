#!/bin/bash

for file in `ls $1`; do
  if ( [[ -d "$1/$file" ]] ); then
    continue
  fi
  testfile=$1/$file
  testfilebap=$1/out/$file.bap
  testfilesim=$1/out/$file.bap.sim
  testfiletsum=$1/out/$file.tsum
  testfilediff=$1/out/$file.dif
  #if its not a directory then process it using the commands
  echo Processing [$testfile]
  echo Summarizing taint 
  time ./trace_reader_cpp -tsum -i $testfile &> $testfiletsum
  echo Converting to BAP 
  time ./trace_reader_cpp -bap -i $testfile &> $testfilebap
  echo Simplifying BAP Results
  time python parseBAPOutput.py -b -i $testfilebap -o $testfilesim
  echo Finding Differences
  time python finddiff.py -tsum $testfiletsum -bapsim $testfilesim -v &> $testfilediff
done
