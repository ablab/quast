# Hello, emacs: this is a -*- Makefile -*-
# $Id: Makefile,v 1.13 2014/08/18 03:49:06 sfeam Exp $
#
# GNUPLOT Makefile OpenWatcom 1.3 on Win32

# To compile:
#       Either, from ..\src:
#         wmake -f ..\config\makefile.oww
#       or copy this file to ..\src, edit it to your liking, then
#       from ..\src:
#         wmake -f makefile.oww
# 
# History of this file:
# 2004-02-23: Created by massive editing of makefile.nt (and config.nt)
# 2004-04-25: Removed all remaining MS-isms (by peeking at how OW's
#  MS namesake replacement tools generate their command lines)
#

# enable exactly one of these:
VARIANT = PLAIN
#VARIANT = PIPES
#VARIANT = CONSOLE

TOP = ..\..

# Programs: 
# The Microsoft Help compiler:
HCW = "c:\program files\help workshop\hcw.exe"
# ... and the HTML help compiler
HHC_DIR = c:\Program Files\HTML Help Workshop
HHC = "$(HHC_DIR)\hhc.exe"

# OpenWatcom programs:
WCL = wcl386
WCC = wcc386
WLD = wlink
WRC = wrc

# Speed up compilation a bit by running the compiler as a DLL from
# inside wmake:
!ifdef __LOADDLL__
!  loaddll wcc      wccd
!  loaddll wccaxp   wccdaxp
!  loaddll wcc386   wccd386
!  loaddll wpp      wppdi86
!  loaddll wppaxp   wppdaxp
!  loaddll wpp386   wppd386
!  loaddll wlink    wlinkd
!  loaddll wlib     wlibd
!endif

# Where to install the PostScript prologue files, relatively to the executable
# wgnuplot.exe
GNUPLOT_PS_DIR = share/PostScript

# paths for external libs added here... 
LDLIBS = 

# Macros for makefile.all
O=obj
T=$(TOP)\term\
D=$(TOP)\docs\
M=$(TOP)\demo\
S=$(TOP)\src
W=$(S)\win

BINARYFILES=$(M)binary1 $(M)binary2 $(M)binary3

# compiler flags: 
#
# Defines

CPPFLAGS =  -DHAVE_CONFIG_H -D_Windows -DHAVE_CONFIG_H -DUSE_MOUSE -DEAM_OBJECTS
# library-dependent ones may have to be added, like:
# -DHAVE_LIBPNG -DHAVE_LIBPDF 

HELPFILE = wgnuplot.chm

DEFAULT_TARGETS = $(BINARYFILES) $(HELPFILE)

TARGETS_PLAIN=wgnuplot.exe wgnuplot.mnu
EXTRA_CPPFLAGS_PLAIN = 

TARGETS_PIPES=wgnuplot_pipes.exe wgnuplot.mnu
EXTRA_CPPFLAGS_PIPES = -DPIPES=1

TARGETS_CONSOLE=gnuplot.exe
EXTRA_CPPFLAGS_CONSOLE = -DPIPES=1 -DWGP_CONSOLE

ALL_TARGETS = $(DEFAULT_TARGETS) $(TARGETS_PLAIN) $(TARGETS_PIPES) $(TARGETS_CONSOLE)

DEFAULT_TARGETS += $(TARGETS_$(VARIANT))

LDLIBS += LIBRARY htmlhelp.lib

# compiler flags, determined from MS ones by OW's "cl /showwopts"

# -c  : don't link
# -I  : add directory to include search path
# -zq : don't show a version banner message
# -5r : P5-optimized, 386-compatible code, register calling convention
# -zm : put each function in its own section to improve dead code elimination
# -bm : target multi-threaded environment
# -sg : grow the stack if needed
# -zlf: store library usage in .obj files
# -ei : force sizeof(enum) >= sizeof(int)
# -o  : optimization options:   
#    r:   reorder for pipelining
#    i:   inline intrinsics
#    l:   loop optimizations
#    t:   optimize for speed
# -j  : char is signed
# -aa : allow non-const initializers for locals
# -d2 : debug information
WCFLAGS = -zlf -ei -zq -I$(S);$(W) -zm -bm -5r -sg -orilt -j -aa -d2 $(CPPFLAGS) $(OPTS)
# removed: -za99

# ...and here.
# see other terminal defines in term.h
TERMFLAGS = -DGNUPLOT_PS_DIR="$(GNUPLOT_PS_DIR)" -I$(T)

# Note: like makefile.wc, this need the special Watcom-style version of
# makefile.all, because line continuation in WMake is different...
!INCLUDE $(S)\makefile.awc

default: $(DEFAULT_TARGETS) .SYMBOLIC

all: $(ALL_TARGETS) .SYMBOLIC

WINOBJS = winmain.$(O) wgnuplib.$(O) wgraph.$(O) wprinter.$(O) wtext.$(O) &
        wpause.$(O) wmenu.$(O) gpexecute.$(O) screenbuf.$(O)

OBJS = $(COREOBJS) version.$(O) $(WINOBJS)

O=cobj
CONSOLE_OBJS = $+ $(OBJS) $-
O=pobj
PIPE_OBJS = $+ $(OBJS) $-
O=obj

WIN_EXTRA_INPUTS = wgnuplot.res

# extensions:
.EXTENSIONS:
.EXTENSIONS: .exe .cobj .pobj .obj .asm .pas .c .cpp .for

.c: $(S);$(W)

# default rule
.c.obj: .AUTODEPEND
        $(WCC) $(WCFLAGS) $[@

.c.cobj: .AUTODEPEND
		$(WCC) $(EXTRA_CPPFLAGS_CONSOLE) $(WCFLAGS) $[@ -fo=$^@

.c.pobj: .AUTODEPEND
		$(WCC) $(EXTRA_CPPFLAGS_PIPES) $(WCFLAGS) $[@ -fo=$^@

# file-specific rules
wgnuplot.exe: $(OBJS) $(WIN_EXTRA_INPUTS)
        $(WLD) @<<linkopt1.oww $(LDLIBS)
RUNTIME windows=4.0
SYSTEM nt_win
DEBUG ALL
OPTION quiet
FILE {
 $(OBJS)
}
LIBRARY kernel32.lib,user32.lib,gdi32.lib,winspool.lib,comdlg32.lib
LIBRARY comctl32.lib,advapi32.lib,shell32.lib
LIBRARY msimg32.lib
OPTION map
OPTION SYMFILE
NAME $^&
OPTION stack=1M
OPTION resource=wgnuplot.res
OPTION ELIMINATE
REFERENCE _wstart_ 
OPTION START=_wstart_
<<

wgnuplot_pipes.exe: $(PIPE_OBJS) $(WIN_EXTRA_INPUTS)
        $(WLD) @<<linkopt1.oww $(LDLIBS)
RUNTIME windows=4.0
SYSTEM nt_win
DEBUG ALL
OPTION quiet
FILE {
 $(PIPE_OBJS)
}
LIBRARY kernel32.lib,user32.lib,gdi32.lib,winspool.lib,comdlg32.lib
LIBRARY advapi32.lib,shell32.lib
LIBRARY msimg32.lib
OPTION map
OPTION SYMFILE
NAME $^&
OPTION stack=1M
OPTION resource=wgnuplot.res
OPTION ELIMINATE
REFERENCE _wstart_ 
OPTION START=_wstart_
<<

gnuplot.exe: $(CONSOLE_OBJS) $(WIN_EXTRA_INPUTS) 
        $(WLD) @<<linkopt1.oww $(LDLIBS)
RUNTIME console=4.0
SYSTEM nt
DEBUG ALL
OPTION quiet
FILE {
 $(CONSOLE_OBJS)
}
LIBRARY kernel32.lib,user32.lib,gdi32.lib,winspool.lib,comdlg32.lib
LIBRARY advapi32.lib,shell32.lib
LIBRARY msimg32.lib
OPTION map
OPTION SYMFILE
NAME gnuplot.exe
OPTION stack=1M
OPTION resource=wgnuplot.res
OPTION ELIMINATE
#REFERENCE _wstart_ 
#OPTION START=_wstart_
<<keep

# NOTE: Unknown what /l 0x409 does in MS RC call --- seems to work without, though
wgnuplot.res :  $(W)\wgnuplot.rc $(W)\wgnuplib.rc &
  $(W)\wresourc.h $(W)\texticon.ico $(W)\grpicon.ico
        $(WRC) -r -q -bt=nt -fowgnuplot.res -i=$(W) &
	   -d_WIN32 -dNDEBUG -dWIN32 $(W)\wgnuplot.rc

term.obj: $(S)\term.c $(CORETERM) .AUTODEPEND
        $(WCC) $(WCFLAGS) $(TERMFLAGS) $[@
term.cobj: $(S)\term.c $(CORETERM) .AUTODEPEND
        $(WCC) $(EXTRA_CPPFLAGS_CONSOLE) $(WCFLAGS) $(TERMFLAGS) $[@ -fo=$^@
term.pobj: $(S)\term.c $(CORETERM) .AUTODEPEND
        $(WCC) $(EXTRA_CPPFLAGS_PIPES) $(WCFLAGS) $(TERMFLAGS) $[@ -fo=$^@

wgnuplot.mnu: $(W)\wgnuplot.mnu
        copy $(W)\wgnuplot.mnu wgnuplot.mnu

pgnuplot.exe: $(W)\pgnuplot.c version.$(O) .AUTODEPEND
        $(WCL) -DWINDOWS_NO_GUI -5r -zq -d2 -fm $(W)\pgnuplot.c -fe=pgnuplot.exe -I$(S) &
	  version.$(O) user32.lib


WD=$(D)windows\
HHP_PROJ=$(WD)wgnuplot.hhp
HHP_CONTENT=$(WD)wgnuplot.hhc
HHP_INPUT=$(WD)wgnuplot.html
HHP_OUTPUT=$(WD)wgnuplot.chm   

wgnuplot.chm: $(HHP_OUTPUT)
        copy $(HHP_OUTPUT) .

$(HHP_OUTPUT): $(HHP_PROJ) $(HHP_INPUT) $(HHP_CONTENT)
        -$(HHC) $(HHP_PROJ)

$(HHP_INPUT) $(HHP_CONTENT): doc2html.exe $(D)gnuplot.doc
        doc2html $(D)gnuplot.doc $(WD)

doc2html.exe: $(WD)doc2html.c $(D)termdoc.c $(D)xref.c version.$(O) .AUTODEPEND
        $(WCL) $(WCFLAGS) -w1 -DWINDOWS_NO_GUI -I$(D);$(T) -fe$^@ $(WD)doc2html.c $(D)termdoc.c $(D)xref.c version.$(O)

gnuplot.gih: doc2gih.exe $(D)gnuplot.doc
        doc2gih $(D)gnuplot.doc $^@

doc2gih.exe: $(D)doc2gih.c $(D)termdoc.c $(D)xref.c .AUTODEPEND
        $(WCL) $(EXTRA_CPPFLAGS_CONSOLE) $(WCFLAGS) -w1 -I$(D);$(T) -fe$^@ $(D)doc2gih.c $(D)termdoc.c $(D)xref.c

#make binary demo files
# WINDOWS_NO_GUI makes sure that wtext.h does not redefine fread() etc.
bf_test.exe : bf_test.c  .AUTODEPEND
        $(WCL) $(WCFLAGS) -w1 -DWINDOWS_NO_GUI -fe$^@ $(S)\bf_test.c

$(BINARYFILES): bf_test.exe
        .\bf_test
        move binary? $(M).

dbinary.$(O): $(S)\binary.c
        $(WCC) $(WCFLAGS) -w1 -DWINDOWS_NO_GUI -fodbinary.$(O) $[@

# clean up temporary files
clean: .SYMBOLIC
        rm -f *.obj *.cobj *.pobj *.err
        rm -f wgnuplot.res $(W)\gnuplot.rtf $(W)\wgnuplot.hlp
        rm -f bf_test.exe linkopt1.oww doc2html.exe
        rm -f doc2gih.exe gnuplot.gih
        rm -f *.map *.sym wgnuplot.gid        

realclean: clean .SYMBOLIC
        rm -f wgnuplot.exe wgnuplot.mnu pgnuplot.exe gnuplot.exe wgnuplot_pipes.exe
        rm -f $(HHP_CONTENT) $(HHP_INPUT) $(HHP_OUTPUT) $(WD)wgnuplot.log
        rm -f wgnuplot.hlp wgnuplot.chm wgnuplot.chw
        rm -f $(D)binary[123] $(D)fit.log $(D)soundfit.par
