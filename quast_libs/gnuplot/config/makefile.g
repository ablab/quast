#
# $Id: makefile.g,v 1.7 2007/11/19 21:17:12 sfeam Exp $
#
# GNUPLOT Makefile for GRASS, a geographic information system. 
#
# To compile, make modifications below (if necessary) then
# % gmake4.1
# % MAKELINKS
#
# NOTE: this creates a binary called 'g.gnuplot' and is located in
#       $GISBASE/bin.  
#       A help file is installed in $(GISBASE)/man/help/g.gnuplot
#
# GRASS driver written by:
# James Darrell McCauley          Department of Ag Engr, Purdue Univ
# mccauley@ecn.purdue.edu         West Lafayette, Indiana 47907-1146
#
# Last modified: 05 Apr 1995
#
# Modification History:
# <15 Jun 1992>	First version created with GNUPLOT 3.2
# <15 Feb 1993> Modified to work with frames
# <16 Feb 1993> Added point types triangle (filled and unfilled), 
#               inverted-triangle (filled and unfilled), 
#               circle (filled and unfilled), and filled box.
#               Graph is no longer erased after g.gnuplot is finished.
# <01 Mar 1993> Modified to work with 3.3b9
# <26 Jun 1993> Fixed up this file to automatically install the 
#               binary and help.
# <05 Apr 1995> Re-worked Gmakefile for version 3.6
#               Cleaned up grass.trm, adding explicit function declarations,
#               so that it compiles cleanly with 'gcc -Wall'
# <14 Apr 1995> adapted for new terminal layout, added font selection
#
#############################################################################
#
# Change REGULAR_FLAGS to be those determined by 'configure' when
# you compiled the plain (non-GRASS) version of gnuplot.
#
# the following is what I use for Solaris 2.3
REGULAR_FLAGS=-DREADLINE=1 -DNOCWDRC=1 -DPROTOTYPES=1 -DHAVE_STRINGIZE=1 \
	-DX11=1 -DHAVE_UNISTD_H=1 -DHAVE_TERMIOS_H=1 -DSTDC_HEADERS=1 \
	-DRETSIGTYPE=void -DGAMMA=lgamma -DHAVE_GETCWD=1 -DHAVE_STRNCASECMP=1 \
	-DXPG3_LOCALE=1 -DHAVE_SYS_SYSTEMINFO_H=1 -DHAVE_SYSINFO=1 \
	-DHAVE_TCGETATTR=1 -I/opt/x11r5/include -g -O
################### Don't touch anything below this line ###################

HELPDEST=$(GISBASE)/man/help/g.gnuplot

# Where to send email about bugs and comments 
EMAIL="mccauley@ecn.purdue.edu\\n\tor grassp-list@moon.cecer.army.mil [info.grass.programmer]"

# Where to ask questions about general usage
HELPMAIL="grassu-list@moon.cecer.army.mil\\n\t[info.grass.user] or gnuplot-info@lists.sourceforge.net [comp.graphics.gnuplot]"
 
# This causes grass.trm to be included in term.h
GTERMFLAGS = -DGISBASE -I. -I./term

EXTRA_CFLAGS=$(GTERMFLAGS) $(REGULAR_FLAGS) -DCONTACT=\"$(EMAIL)\" \
	-DHELPMAIL=\"$(HELPMAIL)\" -DHELPFILE=\"$(HELPDEST)\"

# List of object files (including version.o)
OBJS = alloc.o binary.o bitmap.o command.o contour.o datafile.o dynarray.o \
	eval.o fit.o graphics.o graph3d.o help.o hidden3d.o history.o \
	internal.o interpol.o matrix.o misc.o parse.o plot.o plot2d.o \
	plot3d.o readline.o save.o scanner.o set.o show.o specfun.o \
	standard.o tabulate.o term.o time.o unset.o util.o util3d.o variable.o version.o

all: $(BIN_MAIN_CMD)/g.gnuplot $(GISBASE)/man/help/g.gnuplot

$(BIN_MAIN_CMD)/g.gnuplot: $(OBJS) $(DISPLAYLIB) $(RASTERLIB) $(GISLIB) 
#g.gnuplot: $(OBJS) $(DISPLAYLIB) $(RASTERLIB) $(GISLIB) 
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(DISPLAYLIB) $(RASTERLIB) $(GISLIB) $(TERMLIB) $(MATHLIB)

$(GISBASE)/man/help/g.gnuplot:
	/bin/cp docs/gnuplot.gih $(HELPDEST)


################################################################
# Dependencies

term.o: term.h term.c 

$(OBJS): plot.h

command.o: command.c fit.h

command.o help.o misc.o: help.h

command.o graphics.o graph3d.o misc.o plot.o set.o show.o term.o: setshow.h

fit.o: fit.c fit.h matrix.h plot.h

matrix.o: matrix.c matrix.h fit.h

bitmap.o term.o: bitmap.h

variable.o: variable.c plot.h variable.h

################################################################
$(RASTERLIB): #
$(DISPLAYLIB): #
$(GISLIB): #
