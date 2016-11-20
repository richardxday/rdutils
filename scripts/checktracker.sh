#!/bin/sh
DATADIR=~/Dropbox/AutoSafe
cd $DATADIR
test -e list.txt || touch list.txt
ls -1 *.srt >newlist.txt
if ! diff list.txt newlist.txt >/dev/null 2>&1 ; then
	echo "Updating files"
	srttracker .
	cp newlist.txt list.txt
fi
