# Hey, Emacs, this is a -*- Makefile -*- #
# $Id: Makefile,v 1.10 2014/08/18 03:49:06 sfeam Exp $
#
#
# GNUPLOT Makefile for Cygwin on WinNT and Win95/98/2000
#
# To compile gnuplot for WinXX:
#
# - check the beginning of this file settings you may want to change
#   You'll have to tell it where to find the Help Compiler, among other
#   things.
# - compile the package: go to directory 'gnuplot' and issue the commands
#	 make -C config/cygwin
#	 make install -C config/cygwin
# - resource compiler used is windres.exe in Cygwin
# - note pdf, png, and gif libraries if used have to be build for Cygwin
#   with -mno-cygwin option
# - -static used for static linking to libz-w.a and libpng-w.a
#

#
# ************** Begin of Configuration section ************************
#

# Choose target to compile:
# 1. wgnuplot.exe: The usual Windows GUI for input commands.
# 2. wgnuplot_pipes.exe: The usual Windows GUI with support of pipes, so
#    that e.g.  plot '<awk -f preprocess.awk my.dat')  or  x=system('ls -1')
#    can work. Drawback: wgnuplot_pipes.exe keeps attached to a console (DOS)
#    window it was launched from, or it opens a new one.
# 3. gnuplot.exe: Console mode instead of GUI for the text input.
#
TARGET=wgnuplot.exe
#TARGET=wgnuplot_pipes.exe
#TARGET=gnuplot.exe

# Comment out the definition lines to disable the according features:

# GIF, PNG, JPEG device drivers
# Requires gd library. There are two possibilities how to configure these
# drivers for use in gnuplot, depending on the gd version. See README.1ST for
# more details. 
#
# You should compile gnuplot with GD library v2.0 or newer.
# This library writes PNG, GIF and JPEG images.
# If libgd has been compiled with TrueType font support, then you can use
# scaled TrueType fonts in png images. If not, then uncomment FREETYPE.
# Requires GD, PNG and Z libraries, optionally libfreetype.
#
#NEWGD=1
#JPEG=1
#FREETYPE=1

# PDF device driver
# requires PNG and Z libraries based on particular PDF library used
#PDF=1

# DEBUGging support
# creates binaries suitable for debugging. Some bugs may come and go
# as opposed to a production build since we lower the optimization level
#DEBUG=1

# MOUSE support for the windows terminal
MOUSE=1

# Below you can adapt paths according to your software setup:

# Where to place gnuplot.gih helpfile:
HELPFILE = wgnuplot.hlp

# Where to install the PostScript prologue files, relatively to the executable
# wgnuplot.exe
GNUPLOT_PS_DIR = share/PostScript

# Similarly for javascript files for the canvas terminal:
GNUPLOT_JS_DIR = share/js

# Similarly for lua scripts used by the lua terminal
GNUPLOT_LUA_DIR = share/lua

# Destination directory, used in 'make install':
DESTDIR = /cygdrive/c/Apps/gnuplot

# give here the path to Gygwin GCC compiler unless it is already in PATH

#GCCPATH = /bin/
#GCCPATH =

# Do you want some special optimization?
CFLAGS =


# To compile the .hlp file you need hcw either out of Microsoft SDK or MS Help
# Workshop. The latter can be obtained at www.helpmaster.com/help/devaids.htm.
# Put the path to hcw here unless it is already in PATH:
HCWPATH = /cygdrive/c/Program\ Files/Help\ Workshop/
HCW = $(HCWPATH)hcw
# Switches are for HCW 4.03:
HCWFLAG =

# Choose which windres/rc do you want to use (GNU windres or MS RC):
GNU_RC = 1
ifndef GNU_RC
  # If uncommented GNU_RC above and rc.exe not in PATH, then set:
  MSSDK = c:/mssdk
endif

# GNU sort
#GNUSORT = gsort.exe
GNUSORT = /usr/bin/sort

#
# *************** End of Configuration section *************************


#
#         There shouldn't be anything to be changed below this line
# **********************************************************************
#

# PIPES: define if you would prefer support of pipes undef Windows (e.g.
# plot '<awk -f preprocess.awk my.dat'). Drawback: wgnuplot_pipes.exe keeps
# attached to a console=DOS window it was launched from, or it opens a new one.

# CONSOLE: define if you would prefer the console (non-GUI) version of the
# text command window.

# Set PIPES and CONSOLE according to the target
ifeq ($(TARGET),wgnuplot_pipes.exe)
	PIPES=1
endif
ifeq ($(TARGET),gnuplot.exe)
	PIPES=1
	CONSOLE=1
endif


CC = $(GCCPATH)gcc-3
LD = $(CC)

RM = rm -f
CP = cp -p

ifdef DEBUG
  CFLAGS += -g
  LDFLAGS += -g
else
  CFLAGS += -O2
  LDFLAGS += -s
endif

ifdef PIPES
  OPTS += -DPIPES
  LDFLAGS2 = -mconsole
endif

ifdef CONSOLE
  OPTS += -DWGP_CONSOLE
  LDFLAGS2 = -mconsole
endif

TOP = ../..

# macros for makefile.all
O=o
S=$(TOP)/src
W=$(S)/win
T=$(TOP)/term/
D=$(TOP)/docs/
L=docs/latex/
M=$(TOP)/demo/

VPATH = $(S)

TERMFLAGS = -DGNUPLOT_PS_DIR=\"$(GNUPLOT_PS_DIR)\" -DGNUPLOT_JS_DIR=\"$(GNUPLOT_JS_DIR)\" 
TERMFLAGS += -DGNUPLOT_LUA_DIR=\"$(GNUPLOT_LUA_DIR)\" -I$(TOP)/term
TERMLIBS =

CFLAGS += -I. -I$(S) -D_Windows -DCYGWIN -mno-cygwin -DHAVE_CONFIG_H\
	$(OPTS)

# -I/usr/local/include

ifdef MOUSE
  CFLAGS += -DUSE_MOUSE=1 -DWIN_IPC
endif

ifdef NEWGD
  GD=1
  PNG=1
endif

ifdef GD
  CFLAGS += -DHAVE_LIBGD
  TERMLIBS += -lgd
endif

ifdef PNG
  CFLAGS += -DHAVE_LIBPNG
  TERMLIBS += -lpng -lz
endif

ifdef NEWGD
  CFLAGS += -DHAVE_GD_GIF -DGIF_ANIMATION -DHAVE_GD_PNG
ifdef JPEG
  CFLAGS += -DHAVE_GD_JPEG
  TERMLIBS += -ljpeg
endif
ifdef FREETYPE
  CFLAGS += -DHAVE_GD_TTF
  TERMLIBS += -lfreetype
endif
endif

ifdef PDF
  CFLAGS += -DHAVE_LIBPDF
  TERMLIBS += -lpdf-w -ltiff-w
ifndef PNG
  NEED_PNG_W = 1
else
ifndef NEWGD
  NEED_PNG_W = 1
endif
endif
ifdef NEED_PNG_W
  CFLAGS += -DHAVE_LIBPNG
  TERMLIBS += -lpng-w -lz-w
endif
endif

ifdef GNU_RC
  # RC = /bin/windres
  RC  = $(GCCPATH)windres
  RCFLAGS = --include-dir win \
	--define __WIN32__ --define __WIN95__ \
	--define __GNUWIN32__ --define WIN32
  RCOUT = wgplt_res.$(O)
  RES2COFF = echo wgplt_res.$(O)
else
  RC = $(MSSDK)/bin/rc
  RCFLAGS = -v -i$(MSSDK)/include -iwin \
	-dWIN32 -dMSRC
  RCOUT = -fowgnuplot.res
  RES2COFF = res2coff -i wgnuplot.res -o wgplt_res.$(O)
endif


default: $(TARGET) $(HELPFILE) wgnuplot.mnu $(M)bf_test.exe

all: default gnuplot.ps gnuplot.pdf


# include the 'core makefile template'
include $(S)/makefile.all

OBJS = $(COREOBJS) version.$(O) gpexecute.$(O)

WINOBJS = winmain.$(O) wgnuplib.$(O) wgraph.$(O) wprinter.$(O) wtext.$(O) \
	wpause.$(O) wmenu.$(O) screenbuf.$(O)

WINDOWS = makefile.win makefile.nt README.win $(W)/wcommon.h \
	$(W)/wgnuplib.c $(W)/wgnuplib.h $(W)/wgnuplib.rc \
	$(W)/wgnuplot.mnu $(W)/wgraph.c \
	$(W)/winmain.c $(W)/wmenu.c $(W)/wpause.c $(W)/wprinter.c \
	$(W)/wresourc.h $(W)/wtext.c $(W)/wtext.h \
	$(W)/screenbuf.c $(W)/screenbuf.h \
	$(T)win.trm $(W)/grpicon.ico $(W)/texticon.ico


# default rules
.SUFFIXES: .exe .o .c
.c.o:
	$(CC) -c $(CFLAGS) $<

LDLIBS = -L/usr/local/lib -lkernel32 -lgdi32 -lmsimg32 -lcomctl32

$(TARGET): $(OBJS) $(WINOBJS) wgplt_res.$(O)
	$(LD) $(LDFLAGS) $(LDFLAGS2) -mwindows -mno-cygwin -o $@ \
		$(OBJS) $(WINOBJS) wgplt_res.$(O) $(LDLIBS) $(TERMLIBS)

pgnuplot.exe: $(W)/pgnuplot.c version.$(O)
	$(CC) -O2 -s -mno-cygwin -DWINDOWS_NO_GUI -DHAVE_STDBOOL_H -o $@ \
		$(W)/pgnuplot.c version.$(O) -I$(W) -I$(S) -luser32


# Rules:

wgplt_res.$(O):  $(W)/wgnuplot.rc $(W)/wgnuplib.rc $(W)/wresourc.h $(W)/texticon.ico $(W)/grpicon.ico
	$(RC) $(RCFLAGS) $(W)/wgnuplot.rc $(RCOUT) --include-dir=win
	$(RES2COFF)

show.$(O): show.c plot.h setshow.h
	$(CC) -c $(CFLAGS) -DHELPFILE=\"$(HELPFILE)\" -DBINDIR=\"$(bindir)\" $<

term.$(O): term.c term.h plot.h setshow.h bitmap.h $(CORETERM)
	$(CC) -c $(CFLAGS) $(TERMFLAGS) $<

version.$(O): version.c

WINDEPS = $(W)/wgnuplib.h $(W)/wcommon.h $(W)/wresourc.h

winmain.$(O): $(W)/winmain.c $(W)/wgnuplib.h $(W)/wtext.h plot.h
	$(CC) -c $(CFLAGS) -DHELPFILE=\"$(HELPFILE)\"  $(W)/winmain.c

wgnuplib.$(O): $(W)/wgnuplib.c $(WINDEPS)
	$(CC) -c $(CFLAGS) $(W)/wgnuplib.c

wmenu.$(O): $(W)/wmenu.c $(WINDEPS)
	$(CC) -c $(CFLAGS) $(W)/wmenu.c

wtext.$(O): $(W)/wtext.c $(WINDEPS)
	$(CC) -c $(CFLAGS) $(W)/wtext.c

wpause.$(O): $(W)/wpause.c $(WINDEPS)
	$(CC) -c $(CFLAGS) $(W)/wpause.c

wprinter.$(O): $(W)/wprinter.c $(WINDEPS)
	$(CC) -c $(CFLAGS) $(W)/wprinter.c

wgraph.$(O): $(W)/wgraph.c $(WINDEPS)
	$(CC) -c $(CFLAGS) $(W)/wgraph.c

screenbuf.$(O): $(W)/screenbuf.c $(WINDEPS)
	$(CC) -c $(CFLAGS) $(W)/screenbuf.c

wgnuplot.mnu: $(W)/wgnuplot.mnu
	$(CP) $^ $@


#make binary demo files
$(M)bf_test.exe : bf_test.c
	$(LD) $(LDFLAGS) $(CFLAGS) -DWINDOWS_NO_GUI -o $@ $^
	(cd $(M) ; ./bf_test.exe )


#
# Create config.h
#
$(OBJS) $(WINOBJS): config.h
config.h: $(TOP)/config/config.cyg
	cp -p $< $@

# Create documentation in various formats
#
ALL_TERMINALS_DOC=1
ifdef ALL_TERMINALS_DOC
# Generate TeX documentation with the complete list of all terminals
# (gnuplot.tex should be the same on all platforms):
SORT_TERMINALS=1
ifdef SORT_TERMINALS
# sort alphabetically all terminals (note: req. GNU sort, not from MS)
allterm.h: $(CORETERM)
	@echo "Building allterm.h"
	@for e in `egrep "^[ 	]*START_HELP" $(CORETERM) |\
	     $(GNUSORT) -f -t':' -k2` ; do \
	  f=`echo $$e |cut -d\: -f1` ; s=`echo $$e | cut -d\: -f2` ;\
	  sed -n "/^[ 	]*$$s/,/^[ 	]*END_HELP/p" $$f ; \
	done >$@
else
# sequence of terminals according to "ls term/*.trm":
allterm.h: $(CORETERM)
	@echo "Building allterm.h"
	@cat $(T)*.trm > allterm.c
	$(CPP) $(CFLAGS) -I$(T) -DTERM_DRIVER_H -DTERM_HELP allterm.c | \
	  sed '/^ *$$/d;/^#/d' > allterm.h
	@rm -f allterm.c
endif
doc2tex.exe: $(D)doc2tex.c $(D)termdoc.c allterm.h
	$(LD) $(LDFLAGS) -o $@ -DWINDOWS_NO_GUI -DALL_TERM_DOC $(CFLAGS) -I. -I$(D) -I$(T) $(D)doc2tex.c $(D)termdoc.c
else
# Old version: generate documentation with only currently used terminals:
doc2tex.exe: $(D)doc2tex.c $(D)termdoc.c
	$(LD) $(LDFLAGS) -o $@ -DWINDOWS_NO_GUI $(CFLAGS) -I. -I$(D) -I$(T) $^
endif

gnuplot.tex: $(D)gnuplot.doc doc2tex.exe
	doc2tex $(D)gnuplot.doc gnuplot.tex

# Call LaTeX three times to get the toc right.
gnuplot.dvi: gnuplot.tex $(D)titlepag.tex
	cp gnuplot.tex $(D)gp_tex2.tex
	cp $(TOP)/VERSION $(D)
	cd $(D) && latex gp_tex2.tex && latex gp_tex2.tex && latex gp_tex2.tex
	mv $(D)gp_tex2.dvi gnuplot.dvi
	rm -f $(D)gp_tex2.*
gnuplot.ps: gnuplot.dvi
	dvips -o gnuplot.ps gnuplot.dvi
gnuplot.pdf: gnuplot.tex $(D)titlepag.tex
	cp gnuplot.tex $(D)gp_tex2.tex
	cp $(TOP)/VERSION $(D)
	cd $(D) && pdflatex gp_tex2.tex && pdflatex gp_tex2.tex \
	 	&& pdflatex gp_tex2.tex
	mv $(D)gp_tex2.pdf gnuplot.pdf
	rm -f $(D)gp_tex2.*


# clean up temporary files
clean:
	$(RM) config.h *.$(O) wgnuplot.map wgnuplot.res $(W)/gnuplot.rtf
	$(RM) $(W)/wgnuplib.res wgnuplib.map wgnuplot.lib
	$(RM) $(M)bf_test.exe allterm.h allterm.c
	$(RM) gnuplot.tex gnuplot.dvi gnuplot.ps gnuplot.pdf

realclean: veryclean
veryclean: clean
	$(RM) wgnuplot.exe wgnuplot_pipes.exe gnuplot.exe
	$(RM) wgnuplot.hlp wgnuplot.gid wgnuplot.mnu
	$(RM) $(M)binary[123] $(M)fit.log $(M)soundfit.par

# now move the whole stuff to its destination
install: default
	mkdir -p $(DESTDIR)
	cp gnuplot.exe $(DESTDIR)/gnuplot.exe
	cp wgnuplot.exe $(DESTDIR)/wgnuplot.exe
	cp wgnuplot_pipes.exe $(DESTDIR)/wgnuplot_pipes.exe
	cp $(W)/wgnuplot.mnu $(DESTDIR)/wgnuplot.mnu
	cp wgnuplot.hlp $(DESTDIR)/wgnuplot.hlp
	mkdir -p $(DESTDIR)/$(GNUPLOT_PS_DIR)
	cp $(T)PostScript/*.ps $(DESTDIR)/$(GNUPLOT_PS_DIR)
	mkdir -p $(DESTDIR)/$(GNUPLOT_LUA_DIR)
	cp $(T)lua/* $(DESTDIR)/$(GNUPLOT_LUA_DIR)
	mkdir -p $(DESTDIR)/$(GNUPLOT_JS_DIR)
	cp $(T)js/* $(DESTDIR)/$(GNUPLOT_JS_DIR)

