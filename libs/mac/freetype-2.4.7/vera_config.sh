#!/bin/bash

export MACOSX_DEPLOYMENT_TARGET=10.6
./configure --with-old-mac-fonts CFLAGS="-Os -arch i386"
