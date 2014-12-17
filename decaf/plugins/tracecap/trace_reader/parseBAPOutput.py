'''
 * Copyright (C) <2013> <Syracuse System Security (Sycure) Lab>
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
'''

'''
 @Author Lok Yan
 @date 1 JAN 2013
'''

import re
import sys
import argparse
import collections
import array
import copy 

def toTaintString(taintarray, bBitTaint) :
  s = "0x"
  if bBitTaint :
    taintarray.reverse()
    s = "0x{0:08x}".format(int(taintarray.tostring(),2))
  else :
    for i in taintarray[::-1] :
      if i == '1' :
        s += "FF"
      else :
        s += "00"
  return (s)

parser = argparse.ArgumentParser(description="Input parser")
parser.add_argument('-i', nargs=1, help="input file", type=argparse.FileType('r'), dest='inf', required='true')
parser.add_argument('-o', nargs='?', help="output file", type=argparse.FileType('w'), default=sys.stdout, dest='outf')
parser.add_argument('-m', action='store_true', dest='needModel')
parser.add_argument('-v', action='store_true', dest='verbose')
parser.add_argument('-s', help="Seek to position of file before processing", type=int, dest='seekpos')
parser.add_argument('-sl', help="Skip so many lines from the beginning", type=int, dest='lineskip')
parser.add_argument('-c', help="Only process so many instructions", type=int, dest='count')
parser.add_argument('-b', help="Bitwise taint", action='store_true', dest='bitTaint')

args = parser.parse_args()
inf = args.inf[0]
outf = args.outf
needModel = args.needModel
verbose = args.verbose

if (args.seekpos) : inf.seek(args.seekpos)

state = 0
insn_num = 0
insn = ""
query = ""
result = ""
model = ""
repeat = 0
count = 0
linecount = 0
poscount = 0
taintarray = array.array('c')

if (args.bitTaint) :
  for i in range(0,32) : taintarray.append('0')
else :
  for i in range(0,4) : taintarray.append('0')

curtaint = dict()

for line in inf :
  linecount += 1
  if ( (args.lineskip) and (args.lineskip >= linecount) ) :
    continue 
  if re.match("^\*+$", line) :
    state = 0
    repeat = 0
    for k in curtaint :
      outf.write("  TAINT: (" + insn_num + ") " + k + "  ->  " + toTaintString(curtaint[k], args.bitTaint) + "\n")
    curtaint.clear()
    if ( (args.count) and (count >= args.count) ) :
      break

  if (state == 0) :
    res = re.search("^\*\*\*\*\*\*\*\*\s+([0-9]+)\s+\(.+\)\s*\*+$", line)
    if res :
      insn_num = res.group(1)
      state += 1
  elif (state == 1) :
    if re.search("^-+$", line) :
      state = 3
    else :
      insn = line
      state = 2
  elif (state == 2) :
    if re.search("^-+$", line) :
      state += 1
  elif (state == 3) :
    res = re.search("^// Query for .+\[([0-9]+)\] of (.+)$", line)
    if res :  
      poscount = int(res.group(1))
      reg = res.group(2)
      query = line.strip()
      model = ""
      state += 1
  elif (state == 4) :
    res = re.search("^Model:$", line)
    if res :
      state = 5 
    else :
      res = re.search("^Solve result: (.+)$", line)
      if res :
        result = res.group(1)
        if not reg in curtaint : curtaint.setdefault(reg, copy.deepcopy(taintarray))
        if result[0] == "I" : curtaint[reg][poscount] = '1'
        elif result[0] == "V" : curtaint[reg][poscount] = '0'
        else : 
          print result
          BAD 
        if not repeat :
          if (verbose) : outf.write("(" + insn_num + ") " + insn)
          repeat = 1
          count += 1
        if (verbose) :
          outf.write(query + " -> " + result + "\n")
        if (needModel) :
          outf.write(model)
        #print result
        #print reg
        #print poscount
        state = 6
  elif (state == 5) : 
    res = re.search("^Solve result: (.+)$", line)
    if res :
      result = res.group(1) 
      if not reg in curtaint : curtaint.setdefault(reg, copy.deepcopy(taintarray))
      if result[0] == "I" : curtaint[reg][poscount] = '1'
      elif result[0] == "V" : curtaint[reg][poscount] = '0'
      else : BAD 
      if not repeat :
        if (verbose) : outf.write("(" + insn_num + ") " + insn)
        repeat = 1
        count += 1
      if (verbose) : 
        outf.write(query + " -> " + result + "\n")
      if (needModel) :
        outf.write(model)
      state = 6
      #print result
      #print reg
      #print poscount
    else :
      model += "  " + line
  elif (state == 6) :
    if re.search("^-+$", line) :
      state = 3
  else :
     print ("Should not be here")
     state = 0

