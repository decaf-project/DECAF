import sys
if(sys.argv[1] == sys.argv[2]):
    print "\n\nVersion",sys.argv[1],"is already on the server.\n\n"
    sys.exit(-1)
sys.exit(0)
