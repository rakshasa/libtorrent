#!/bin/sh

mkdir $1
cd $1
nice -n11 btdownloadheadless.py --max_upload_rate $1 --url file:///tmp/bt/t.torrent > /dev/null &
