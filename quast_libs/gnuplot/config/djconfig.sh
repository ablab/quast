#! /bin/sh
#
# This script configure GNUPLOT-3.7 for building with DJGPP
# (You need bash and many other packages installed for that.
# for novices it's recommended to use Makefile.dj2 instead)
#
# You need also libgd.a, libpng.a and libz.a installed with 
# corresponding include files (otherwise GIF and PNG terminals
# support may not be available)
#
# After running this script simply type
#	make
# to build GNUPLOT
# Finally copy gnuplot.exe and docs/gnuplot.gih to one directory on
# DOS path and enjoy (You can strip gnuplot.exe before)
#-------------------------------------------------------------------
# where are the sources? If you use (like I) to have the sources
# in a separate directory and the objects in an other, then set
# here the full path to the source directory and run this script
# in the directory where you want to build gnuplot!!
srcdir=..
#
# give now the configure script some hints
#
export PATH_SEPARATOR=:
export CC=gcc
export LD=ld
export ac_cv_path_install="ginstall -c"
export CONFIG_SHELL=bash
#
$srcdir/configure --srcdir=$srcdir --without-x --with-gd --with-png
