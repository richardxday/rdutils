#!/bin/sh
ARGS="--user admin --password arsebark http://localhost:8080/snapshot.cgi -O"
IMAGE="images/Image-`date +%Y-%m-%d-%H-%M-%S`.jpg"

wget $ARGS $IMAGE 2>/dev/null
while true ; do
  sleep 1
  IMAGE1=$IMAGE
  IMAGE="images/Image-`date +%Y-%m-%d-%H-%M-%S`.jpg"
  wget $ARGS $IMAGE 2>/dev/null
  imagediff $IMAGE1 $IMAGE
done
