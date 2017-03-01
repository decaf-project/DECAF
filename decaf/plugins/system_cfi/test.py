import os
f = open("input.txt")
for l in f:
	sys.rename(str(l), str(l).lower())
