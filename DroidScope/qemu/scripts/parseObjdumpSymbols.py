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
 @Date 1 JAN 2013
'''

import re
import sys

inf = open(sys.argv[1], "r")
outf = open(sys.argv[2], "w")

addr = 0
sym = ""

for line in inf:
  res = re.search("^((0x)*[0-9A-Fa-f]+) <(.+)>:$", line)
  if res :
    addr = int(res.group(1), base=16)
    sym = res.group(3)
    outf.write("0x{0:08x}".format(addr) + "," + sym + "\n")
