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

  
parser = argparse.ArgumentParser(description="Input parser")
parser.add_argument('-tsum', nargs=1, help="input file", type=argparse.FileType('r'), dest='tsumf', required='true')
parser.add_argument('-bapsim', nargs=1, help="input file", type=argparse.FileType('r'), dest='simf', required='true')
parser.add_argument('-v', action='store_true', dest='verbose')

args = parser.parse_args()
tsumf = args.tsumf[0]
simf = args.simf[0]

#this procedure only works for the fuzz test

#the idea is to take each input line from simf
# and then search for the corresponding line in tsumf

bapToVineReg = { 
  "R_EAX" : "R@eax",
  "R_EBX" : "R@ebx",
  "R_ECX" : "R@ecx",
  "R_EDX" : "R@edx",
  "R_ESI" : "R@esi",
  "R_EDI" : "R@edi"
  }

insnNum = sys.maxint
reg = ""
taint = ""
insnNum2 = sys.maxint
reg2 = ""
taint2 = ""

history = { sys.maxint : {"DUMMY", "DUMMY"} }

for line in simf :
  res = re.search("TAINT: \(([0-9]+)\) (.+):.+->\s+(.+)$", line)
  if res :
    insnNum = int(res.group(1))
    if res.group(2) in bapToVineReg :
      reg = bapToVineReg[res.group(2)]
    else :
      res2 = re.search("TAINT:.+mem:\?u32\[(0x[0-9,A-F,a-f]+):u32.+\]:u32.+$", line)
      if res2 and res2.group(1) :
        reg = "M@" + res2.group(1) 
      else :
        print "SKIPPED: " + line
        continue
    taint = res.group(3)
    if insnNum in history :
      print history[insnNum]
    else :
      for line2 in tsumf :
        res2 = re.search("TAINT: \(([0-9]+)\) [0-9,a-f,A-F]+ : (.+) .* -> (.+) .*$", line2)
        if res2 :
          insnNum2 = int(res2.group(1))
          reg2 = res2.group(2)
          taint2 = res2.group(3) 
          if (insnNum == insnNum2) and (reg == reg2) :
            if (taint != "0x00000000") and (taint != taint2) :
              print "(" + str(insnNum) + ") " + reg + " : " + taint + " <> " + taint2 + " XOR = " + str(bin(int(taint,16) ^ int(taint2,16)).count('1'))
            elif args.verbose:
              print "(" + str(insnNum) + ") " + reg + " : " + taint + " == " + taint2
            break
          else : #if they are not the same then add it into the history 
            if not insnNum2 in history :
              history[insnNum2] = { reg2:taint2 }
            else :
              history[insnNum2][reg2] = taint2
            if (args.verbose) :
              print str(insnNum2) + " " + reg2 + " " + taint2
    

