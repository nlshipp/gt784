#! /bin/sh

find ./userspace/gpl/ -name aclocal.m4 |xargs touch
find ./userspace/gpl/ -name config.h.in |xargs touch
find ./userspace/gpl/ -name configure | xargs touch
find ./userspace/gpl/ -name config.status |xargs touch

find ./userspace/public/ -name aclocal.m4 |xargs touch
find ./userspace/public/ -name config.h.in |xargs touch
find ./userspace/public/ -name configure | xargs touch
find ./userspace/public/ -name config.status |xargs touch
