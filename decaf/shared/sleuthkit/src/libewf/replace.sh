#!/bin/sh
#

ORIG=$1;
NEW=$2;

echo "Replacing $ORIG in $NEW";

for FILE in *.c;
do
	sed "s/$ORIG/$NEW/g" $FILE > $FILE.tmp;
	rm -f $FILE;
	mv $FILE.tmp $FILE;
done

for FILE in *.h;
do
	sed "s/$ORIG/$NEW/g" $FILE > $FILE.tmp;
	rm -f $FILE;
	mv $FILE.tmp $FILE;
done

