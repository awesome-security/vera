#!/bin/bash

export arch_flags="-arch i386"
#export CC=gcc-4.0
#export CXX=gxx-4.0
#export LD=g++-4.0

./configure CFLAGS="$arch_flags" CXXFLAGS="$arch_flags" CPPFLAGS="$arch_flags" LDFLAGS="$arch_flags" \
	OBJCFLAGS="$arch_flags" OBJCXXFLAGS="$arch_flags" --with-macosx-sdk=/Developer/SDKs/MacOSX10.6.sdk --with-macosx-version-min=10.6 \
	--with-opengl
