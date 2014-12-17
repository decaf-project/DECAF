#!/usr/bin/perl

use warnings;
use strict;

my $argc = @ARGV;

if ($argc < 1) {
    print "Usage: $0 elfbinary\n";
    exit 1;
}

my $binary = $ARGV[0]; 

my $basestr = `objdump -p $binary | grep -A 1 "LOAD off" | grep -B 1 "r-x" | grep "LOAD" | cut -c34-43`;

my $baseaddr = substr($basestr, 0, -1); 

open(SYMBOLS, "nm $binary | grep \" T \" |");
while (<SYMBOLS>) {
    if (m/([0-9a-f]+) T ([^\n]+)/) {
	
	printf "0x%08x %s\n", hex($1)-hex($baseaddr), $2; 
    }
}

