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

inf = open(sys.argv[1], "r")
outf = open(sys.argv[2], "w")

totalregs = -1
inregs = -1
outregs = -1
sym = ""
addr = 0
state = 0

"""
Example
      registers     : 2
      ins           : 1
      outs          : 0
      insns size    : 3 16-bit code units
02e248:                                        |[02e248] com.android.email.Account.getDraftsFolderName:()Ljava/lang/String;
"""

line = inf.readline()
while line :
  res = re.search("^\s+registers\s+:\s([0-9]+)$", line)
  if res : #we found registers
    totalregs = res.group(1)
    line = inf.readline()
    res = re.search("^\s+ins\s+:\s([0-9]+)$", line)
    #I don't check the line since it SHOULD be here
    inregs = res.group(1)
    line = inf.readline()
    res = re.search("^\s+outs\s+:\s([0-9]+)$", line)
    outregs = res.group(1)
    line = inf.readline() #gets rid fo the insn size line
    line = inf.readline()
    res = re.search("\|\[((0x)*[0-9A-Fa-f]+)\] (.+)$", line)
    addr = int(res.group(1), base=16) + 0x28 + 0x10
    #0x28 is the odex header and 0x10 is the method start offset
    sym = res.group(3)
    outf.write("0x{0:08x}".format(addr) + "," + sym + "," + totalregs + "," + inregs + "," + outregs + "\n");
  line = inf.readline()
    
"""
# Long Way

for line in inf :
  if (state == 0) :
    res = re.search("^\s+registers\s+:\s([0-9]+)$", line)
    if res :
      totalregs = res.group(1)
      state += 1
  elif (state == 1) :
    res = re.search("^\s+ins\s+:\s([0-9]+)$", line)
    if res :  
      inregs = res.group(1)
      state += 1
  elif (state == 2) :
    res = re.search("^\s+outs\s+:\s([0-9]+)$", line)
    if res :  
      outregs = res.group(1)
      state += 1
  elif (state == 3) :
    res = re.search("\|\[((0x)*[0-9A-Fa-f]+)\] (.+)$", line)
    if res :
      addr = hex(int(res.group(1), base=16) + 0x28 + 0x10)
      sym = res.group(3)
      outf.write("0x{0:08x}".format(addr) + "," + sym + "," + totalregs + "," + inregs + "," + outregs + "\n");
      state = 0
  else :
    sys.exit(-1)
"""

      
