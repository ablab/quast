# Hello, emacs: this is a -*- Makefile -*-
# $Id: Makefile,v 1.48.2.4 2016/03/22 16:49:48 markisch Exp $
#
# GNUPLOT Makefile for MinGW/MSYS or MinGW-w64/MSYS2 on Windows 
# This assumes a typical installation of the MinGW toolkit, the free
# GNU gcc. 
# See http://www.mingw.org or http://msys2.github.io/ for more details.

# To compile gnuplot for Windows:
# - Change the configuration section in 'config/mingw/Makfile' to
#   match your setup.
# - On the MSYS/MSYS2 prompt, change directory to 'config/mingw' and issue
#   the commands
#	 make all
#	 make install
#
# This makefile was tested with 
#   MSYS, MinGW32 and Microsoft HTML Help 1.4 SDK.
#   MSYS2, MinGW-w64


#
# ************** Begin of Configuration section ************************
#

# Comment out the definition lines to disable the according features:

# Define this only if doing a release build
RELEASE=1

# Set-up your compiler environment
#
# Define if you are using MSYS2/MinGW64 instead of MSYS/MinGW
#MINGW64=1
# Define if you want a 32bit version (even using a 64bit compiler)
#M32=1
# Define if you use clang instead of gcc, requires MINGW64
#CLANG=1
# Use the colorgcc frontend explicitely
#COLORGCC=1
# Specify the path to the compiler unless it is already in PATH
#CCPATH = g:/mingw32/bin/
#CCPATH = c:/apps/mingw/bin/

# Path to 32bit programms
# On 32bit machines:
#PROGRAMFILES32="$(PROGRAMFILES)"
#PROGRAMFILES32=/c/Program\ Files
# On 64bit machines: (no access to ProgramFiles(x86) variable!)
PROGRAMFILES32=/c/Program\ Files\ \(x86\)

# GIF, PNG, JPEG device drivers
# Requires gd library. There are two possibilities how to configure these
# drivers for use in gnuplot, depending on the gd version. See README.1ST for
# more details.
#
# You should compile gnuplot with GD library v2.0 or newer.
# This library writes PNG, GIF and JPEG images.
# If libgd has been compiled with TrueType font support, then you can use
# scaled TrueType fonts. If not, then uncomment FREETYPE.
# Requires GD, PNG and Z libraries, optionally libfreetype.
# In some cases, libfreetype can depend on additional libraries such as
# fontconfig or iconv; then, uncomment GDAUTOCONFIGLIBS so that all of the
# libraries needed for linking will be taken by running 'gdlib-config'.
# Uncomment BGD if you want to use the binary DLL version of libgd.
#NEWGD=1
#JPEG=1
#FREETYPE=1
#GDAUTOCONFIGLIBS=1
#BGD=1

# PDF device driver
# Requires PNG and Z libraries based on particular PDF library used, and
# optionally also TIFF library according to
# 1. defined PDF_NOTIFF:
#    'Light' pdf library (produces wgnuplot.exe only 200 KB larger!)
#    You can get this pdf library by compiling it without PNG, GD and TIFF
#    support: change  pdflib/p_intern.h  and recompile the pdf library.
#    Gnuplot does not use the tiff routines, and gd/png are elsewhere.
# 2. undefined PDF_NOTIFF:
#    Full pdf library (produces wgnuplot.exe 500 KB larger!)
#PDF=1
#PDF_NOTIFF=1

# DEBUGging support
# creates binaries suitable for debugging. Some bugs may come and go
# as opposed to a production build since we lower the optimization level
#DEBUG=1

# MOUSE support for the windows terminal
MOUSE=1

# Cairo/Pango terminals
# Uncomment the following lines to select the pngcairo and pdfcairo
# terminals
#CAIROTERMS=1
#CAIROLIBS=1

# WXWIDGETS interactive terminal
# Requires wxWidgets, Cairo, Pango, and their respective dependencies.
# Uncomment the following lines to compile it.
#WXT=1
#CAIROLIBS=1

# Build qt terminal?
#QT = 1
# specify QT installation directory
#QT_DIR = /c/Qt/5.2.1/mingw48_32
ifndef MINGW64
QT_DIR = /d/Source/Qt-5.2.1-mingw/5.2.1/mingw48_32
else
QT_DIR = /mingw64
endif

# Uncomment the following line to select the lua/tikz terminal
#LUA=lua

# Uncomment the following line to select the caca terminal
# Requires libcaca version 0.99.beta15 or newer. A post-beta18
# svn version is recommended since it contains an improved
# win32 backend.
CACA=1

# Uncomment if you have libiconv
ICONV=1

# Uncomment if you have libcerf
# http://sourceforge.net/projects/libcerf/
CERF=1

# Below you can adapt paths according to your software setup:

# Where to install the PostScript prologue files, relatively to the
# executable in bin/wgnuplot.exe
GNUPLOT_PS_DIR = share/PostScript

# Similarly for javascript files for the canvas terminal:
GNUPLOT_JS_DIR = share/js

# Similarly for translation files for the qt terminal:
GNUPLOT_QT_DIR = share/qt

# Similarly for scripts used by the lua terminal
GNUPLOT_LUA_DIR = share/lua

# Install will put TeX files here:
GNUPLOT_TEX_DIR = share/texmf/tex

# gnuplot will look for gnuplotrc here:
GNUPLOT_SHARE_DIR = share


# Destination directory, used in 'make install':
#DESTDIR = /local/opt/gnuplot-4.5
DESTDIR = "$(PROGRAMFILES)/gnuplot"

# Do you want some special optimization / flags?
#CFLAGS +=
ifdef CLANG
CFLAGS += -fcolor-diagnostics -fansi-escape-codes
CFLAGS += -Wno-error=incompatible-ms-struct -Wno-ignored-attributes
CFLAGS +=  -isystem c:/msys64/mingw64/include -pedantic
endif
ifdef MINGW64
# This does not seem to work even if GCC_COLORS is set.
#CFLAGS += -fdiagnostics-color=always
endif

# To compile the .chm file you need the Microsoft HTML Help 1.4 SDK
# It can be obtained here:
# http://go.microsoft.com/fwlink/?LinkId=154968
# We need to explicitly set the correct path here since we also need
# adjust the include and library paths.
HHWPATH = $(PROGRAMFILES32)/HTML\ Help\ Workshop/
HHC = $(HHWPATH)hhc

# Choose which resource compiler you want to use (GNU windres or MS rc):
GNU_RC = 1
ifndef GNU_RC
  # If uncommented GNU_RC above and rc.exe not in PATH, then set:
  MSSDK = c:/mssdk
endif

# GNU sort
GNUSORT = /bin/sort

# Inno Setup Compiler
# get it from: http://www.jrsoftware.org/isdl.php
ISCC = $(PROGRAMFILES32)/Inno\ Setup\ 5/iscc

# Directory that might contain extra files to be shipped with the installer.
# This should include dependencies like DLLs, but also fontconfig configuration files.
ifdef MINGW64
ifdef M32
#EXTRADIST = /d/gnuplot/dist32
else
#EXTRADIST = /d/gnuplot/dist64
endif
else
#EXTRADIST = /d/gnuplot/dist
endif

#
# *************** End of Configuration section *************************
#


#         There shouldn't be anything to be changed below this line
# **********************************************************************
#

# Choose target to compile:
# 1. wgnuplot.exe: The usual Windows GUI for input commands.
# 2. wgnuplot_pipes.exe: The usual Windows GUI with support of pipes, so
#    that e.g.  plot '<awk -f preprocess.awk my.dat')  or  x=system('ls -1')
#    can work. Drawback: wgnuplot_pipes.exe keeps attached to a console (DOS)
#    window it was launched from, or it opens a new one.
# 3. gnuplot.exe: Console mode instead of GUI for the text input.
#

# PIPES: define if you would prefer support of pipes undef Windows (e.g.
# plot '<awk -f preprocess.awk my.dat'). Drawback: wgnuplot_pipes.exe keeps
# attached to a console=DOS window it was launched from, or it opens a new one.

# CONSOLE: define if you would prefer the console (non-GUI) version of the
# text command window.

ifndef TARGET
TARGET=wgnuplot.exe
#TARGET=wgnuplot_pipes.exe
#TARGET=gnuplot.exe
endif

# Set PIPES and CONSOLE according to the target
ifeq ($(TARGET),wgnuplot.exe)
	MENUFILE=wgnuplot.mnu
	O=o
	CFLAGSEXTRA=-DUSE_FAKEPIPES
endif
ifeq ($(TARGET),wgnuplot_pipes.exe)
	MENUFILE=wgnuplot.mnu
	PIPES=1
	O=po
endif
ifeq ($(TARGET),gnuplot.exe)
	PIPES=1
	CONSOLE=1
	O=co
endif


ifdef MINGW64
ifdef M32
CFLAGS += -m32
else
CFLAGS += -m64
endif
else
# MINGW32 always produces 32bit executables
ifndef M32
M32=1
endif
endif

ifndef CLANG
ifdef COLORGCC
CC=$(CCPATH)color-gcc
CXX=$(CCPATH)color-g++
else # !COLORGCC
ifdef (CCPATH)
CC = $(CCPATH)gcc
CXX = $(CCPATH)g++
else #!CCPATH
CC = gcc
CXX = g++
endif
endif
else # CLANG
CC = $(CCPATH)clang
CXX = $(CCPATH)clang++
endif
LD = $(CC)
LDX = $(CXX)

RM = rm -f
CP = cp -p

ifdef DEBUG
  CFLAGS += -g -Wall -pipe
  LDFLAGS += -g
else
  CFLAGS += -O2 -pipe
  LDFLAGS += -s
endif

LDFLAGS2 = -mwindows -Wl,--enable-auto-import
ifdef PIPES
  OPTS += -DPIPES
  LDFLAGS2 = -mconsole
endif

ifdef CONSOLE
  OPTS += -DWGP_CONSOLE -DCONSOLE_SWITCH_CP
  LDFLAGS2 = -mconsole
endif

TOP = ../..

# macros for makefile.all
#O has been defined above already
#O=o
S=$(TOP)/src
W=$(S)/win
T=$(TOP)/term/
D=$(TOP)/docs/
M=$(TOP)/demo/

VPATH = $(S):$(S)/wxterminal

TERMFLAGS = -DGNUPLOT_PS_DIR=\"$(GNUPLOT_PS_DIR)\" -DGNUPLOT_JS_DIR=\"$(GNUPLOT_JS_DIR)\"
TERMFLAGS += -I$(TOP)/term

TERMLIBS =

CFLAGS += -I. -I$(S) -I$(T) -D_Windows -DHAVE_CONFIG_H $(OPTS)
CFLAGS += -D__USE_MINGW_ANSI_STDIO=1
CFLAGS += -DGNUPLOT_SHARE_DIR=\"$(GNUPLOT_SHARE_DIR)\"

ifndef RELEASE
  CFLAGS += -DDEVELOPMENT_VERSION
endif

ifdef MOUSE
  CFLAGS += -DUSE_MOUSE=1 -DWIN_IPC
endif

# Name of helpfiles:
HELPFILE = wgnuplot.chm
HELPFILEJA = wgnuplot-ja.chm
CFLAGS += -I$(HHWPATH)include
BUILDHELPFILE = $(D)windows/$(HELPFILE)

ifdef NEWGD
  GD=1
  PNG=1
endif

ifdef GD
  CFLAGS += -DHAVE_LIBGD
  ifndef BGD
    TERMLIBS += -lgd
  else
    TERMLIBS += -lbgd
  endif
endif

ifdef PNG
  CFLAGS += -DHAVE_LIBPNG
  ifndef BGD
  ifndef GDAUTOCONFIGLIBS
    TERMLIBS += -lpng -lz
  endif
  endif
endif

ifdef NEWGD
  CFLAGS += -DHAVE_LIBGD -DHAVE_GD_H -DHAVE_GD_GIF -DGIF_ANIMATION -DHAVE_GD_PNG
ifdef JPEG
  CFLAGS += -DHAVE_GD_JPEG
  ifndef BGD
  ifndef GDAUTOCONFIGLIBS
    TERMLIBS += -ljpeg
  endif
  endif
endif
ifdef FREETYPE
  CFLAGS += -DHAVE_GD_TTF
  ifndef BGD
  ifndef GDAUTOCONFIGLIBS
    TERMLIBS += -lfreetype
  endif
  endif
endif
ifdef GDAUTOCONFIGLIBS
  TERMLIBS += $(shell gdlib-config --libs)
endif
endif

ifdef PDF
  CFLAGS += -DHAVE_LIBPDF
ifdef PDF_NOTIFF
  TERMLIBS += -lpdf_notiff
else
  TERMLIBS += -lpdf -ltiff
#  TERMLIBS += -lpdf-w -ltiff-w
endif
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

ifdef CAIROLIBS
	CAIRO_CFLAGS := $(shell pkg-config --cflags cairo | sed s_-Ic:_-I/c_g)
	PANGOCAIRO_CFLAGS := $(shell pkg-config --cflags pangocairo cairo | sed s_-Ic:_-I/c_g)
	PANGOCAIRO_LIBS := $(shell pkg-config --libs pangocairo cairo | sed s_-Lc:_-L/c_g)
	CAIRO_OBJS := gp_cairo.$(O) gp_cairo_helpers.$(O)
	TERMFLAGS += $(PANGOCAIRO_CFLAGS)
endif

ifdef CAIROTERMS
	CFLAGS += -DHAVE_CAIROPDF
	TERMFLAGS += -DHAVE_CAIROEPS
endif

ifdef WXT
	CFLAGS += -DWXWIDGETS
	WXT_CXXFLAGS := $(shell wx-config --cxxflags)
	CXXFLAGS += $(WXT_CXXFLAGS)
	WX_LIBS := $(shell wx-config --libs | sed -e "s+-Wl,--subsystem,windows++g" -e "s+-mwindows++g")
	WX_OBJS = wxt_gui.$(O)
endif

ifdef QT
	QT_LIBS = -lQt5Core -lQt5Gui -lQt5Network -lQt5Svg -lQt5Widgets -lQt5PrintSupport -lqtmain
	QT_CXXFLAGS =  -I$(QT_DIR)/include -I$(QT_DIR)/include/Qt -I$(QT_DIR)/include/QtCore -I$(QT_DIR)/include/QtGui
	QT_CXXFLAGS += -I$(QT_DIR)/include/QtWidgets -I$(QT_DIR)/include/QtNetwork
	QT_CXXFLAGS += -I$(QT_DIR)/include/QtSvg -DQTGNUPLOT_DATA_DIR=\"$(GNUPLOT_QT_DIR)\"
	QT_LDFLAGS = -L$(QT_DIR)/lib
#
	CFLAGS += -DQTTERM -DQT_NO_OPENGL
	TERMLIBS += $(QT_LIBS)
	CXXFLAGS += $(QT_CXXFLAGS)
	LDFLAGS  += $(QT_LDFLAGS)
	QT_OBJS = qt_term.$(O) 
	GNUPLOTQTOBJS = gnuplot_qt.o QtGnuplotWindow.o \
		QtGnuplotApplication.o QtGnuplotWidget.o \
		QtGnuplotScene.o QtGnuplotItems.o \
		QtGnuplotEvent.o \
		moc_QtGnuplotWindow.o moc_QtGnuplotApplication.o \
		moc_QtGnuplotWidget.o moc_QtGnuplotScene.o \
		moc_QtGnuplotEvent.o qrc_QtGnuplotResource.o
#
	MOC = $(QT_DIR)/bin/moc
	UIC = $(QT_DIR)/bin/uic
	RCC = $(QT_DIR)/bin/rcc
	LRELEASE = $(QT_DIR)/bin/lrelease
	QT_FILES_TARGETS  = qrc_QtGnuplotResource.cpp ui_QtGnuplotSettings.h moc_QtGnuplotWindow.cpp moc_QtGnuplotApplication.cpp 
	QT_FILES_TARGETS += moc_QtGnuplotWidget.cpp moc_QtGnuplotScene.cpp moc_QtGnuplotEvent.cpp
	QT_TRANSLATION = qtgnuplot_fr.qm qtgnuplot_ja.qm
	QT_TARGET = gnuplot_qt.exe
endif

ifdef LUA
	CFLAGS += -DHAVE_LUA
	TERMLIBS += -l$(LUA)
	TERMFLAGS += -DGNUPLOT_LUA_DIR=\"$(GNUPLOT_LUA_DIR)\"
	LUATARGETS += gnuplot-lua-tikz.sty
	LUA_HELP = gnuplot-tikz.help
endif

ifdef CACA
	CFLAGS += -DHAVE_LIBCACA
	TERMLIBS += -lcaca
endif

ifdef ICONV
	CFLAGS += -DHAVE_ICONV
	TERMLIBS += -liconv
endif

ifdef CERF
	CFLAGS += -DHAVE_LIBCERF -DNEED_CEXP
	TERMLIBS += -lcerf -lm
endif

ifdef GNU_RC
  # RC = d:/cygnus/cygwin-b20/H-i586-cygwin32/bin/windres
  RC  = $(CCPATH)windres
  RCFLAGS = --include-dir=$(W) \
	--define __WIN32__ --define __WIN95__ \
	--define __GNUWIN32__ --use-temp-file
  RCOUT = wgplt_res.$(O)
  RES2COFF = echo wgplt_res.$(O)
else
  RC = $(MSSDK)/bin/rc
  RCFLAGS = -v -i$(MSSDK)/include -i$(W) -dWIN32
  RCOUT = -fowgnuplot.res
  RES2COFF = res2coff -i wgnuplot.res -o wgplt_res.$(O)
endif

ifdef RELEASE
VERSIONING = $(S)/version.c
else
VERSIONING = timestamp.h
endif

PATCHLEVEL := `cat $(TOP)/PATCHLEVEL`
MAINVERSION := `cat $(TOP)/VERSION`
SHORTVERSION := `cut -b 2 --complement $(TOP)/VERSION`$(PATCHLEVEL)
NUMBERS := 0 1 2 3 4 5 6 7 8 9
ifeq ($(strip $(filter-out $(NUMBERS),$(PATCHLEVEL))),)
LONGVERSION := $(MAINVERSION).$(PATCHLEVEL)
else
LONGVERSION := $(MAINVERSION).0
endif
FULLVERSION := $(MAINVERSION) patchlevel $(PATCHLEVEL)


default: $(TARGET) $(MENUFILE) support

console: 
	$(MAKE) TARGET=gnuplot.exe default

windows:
	$(MAKE) TARGET=wgnuplot.exe default

pipes:
	$(MAKE) TARGET=wgnuplot_pipes.exe default

docs:
	$(MAKE) TARGET=gnuplot.exe gnuplot.pdf gpcard.pdf tutorial.pdf \
		ps_symbols.pdf ps_fontfile_doc.pdf ps_guide.pdf

support: $(HELPFILE) $(M)bf_test.exe $(QT_TARGET) $(QT_TRANSLATION) demo_plugin.dll $(LUATARGETS) $(HELPFILEJA)

ja: $(HELPFILEJA)

all: console windows pipes support docs


# include the 'core makefile template'
include $(S)/makefile.all

OBJS = $(COREOBJS) version.$(O) gpexecute.$(O) $(WX_OBJS) $(CAIRO_OBJS) $(QT_OBJS)

WINOBJS = winmain.$(O) wgnuplib.$(O) wgraph.$(O) wprinter.$(O) \
	  wpause.$(O) wgdiplus.$(O)
ifneq ($(TARGET),gnuplot.exe)
WINOBJS += wtext.$(O) screenbuf.$(O) wmenu.$(O) wredirect.$(O)
endif

# default rules
.SUFFIXES: .exe .$(O) .c
.c.$(O):
	$(CC) -c $(CFLAGS) $(CFLAGSEXTRA) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

%.$(O): $W/%.c
	$(CC) -c $(CFLAGS) $(CFLAGSEXTRA) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

LDLIBS = -lkernel32 -lgdi32 -lwinspool -lcomdlg32 -lcomctl32 -ladvapi32 -lshell32 -lmsimg32 -lgdiplus
LDLIBS += -lhtmlhelp
LDFLAGS2 += -L$(HHWPATH)lib
ifdef MINGW64
LDFLAGS2 += -L.
endif

ifndef M32
$(TARGET): $(VERSIONING) $(OBJS) $(WINOBJS) wgplt_res.$(O) Makefile htmlhelp.lib
else
$(TARGET): $(VERSIONING) $(OBJS) $(WINOBJS) wgplt_res.$(O) Makefile
endif
	$(LDX) $(LDFLAGS) $(LDFLAGS2) -o $@ $(OBJS) $(WINOBJS) wgplt_res.$(O) $(LDLIBS)\
	$(TERMLIBS) $(WX_LIBS) $(PANGOCAIRO_LIBS)

pgnuplot.exe: $(W)/pgnuplot.c version.$(O)
	$(CC) -O2 -DWINDOWS_NO_GUI -DHAVE_STDBOOL_H -s -o $@ $^ -I$(W) -I$(S) -luser32

# pull in dependency info for existing object files
ifndef NODEPS
-include $(OBJS:.$(O)=.d)
-include $(WINOBJS:.$(O)=.d)
endif

# explicit rules

wgplt_res.$(O): $(W)/wgnuplot.rc $(W)/wgnuplib.rc $(W)/wresourc.h $(W)/texticon.ico $(W)/grpicon.ico
	$(RC) $(RCFLAGS) $< $(RCOUT)
	$(RES2COFF)

show.$(O): show.c plot.h setshow.h
	$(CC) -c $(CFLAGS) $(CFLAGSEXTRA) -DBINDIR=\"$(bindir)\" -DGNUPLOT_PS_DIR=\"$(GNUPLOT_PS_DIR)\" -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

term.$(O): term.c term.h plot.h setshow.h bitmap.h $(CORETERM)
	$(CC) -c $(CFLAGS) $(CFLAGSEXTRA) $(TERMFLAGS) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

ifndef RELEASE
timestamp.h: $(TOP)/ChangeLog
	@echo Creating $@
	@echo "#ifndef GNUPLOT_TIMEBASE_H_INCLUDED" >$@t
	@echo "#define GNUPLOT_TIMEBASE_H_INCLUDED" >>$@t
	@head -1 $< | sed -e 's,\(^[0-9-]* \).*$$,const char gnuplot_date[] = "\1";,' >>$@t
	@echo "#endif /* GNUPLOT_TIMEBASE_H_INCLUDED */" >> $@t
	@if cmp -s $@ $@t; then rm -f $@t; else mv $@t $@; fi
else
$(S)/version.c: $(TOP)/VERSION $(TOP)/PATCHLEVEL $(TOP)/ChangeLog $(S)/Makefile.maint
	$(MAKE) -C $(S) srcdir=. top_srcdir=.. -f Makefile.maint version.c
endif

WINDEPS = $(W)/wgnuplib.h $(W)/wcommon.h $(W)/wresourc.h $(W)/screenbuf.h $(W)/wgdiplus.h

winmain.$(O): $(W)/winmain.c $(W)/wgnuplib.h $(W)/wtext.h plot.h
	$(CC) -c $(CFLAGS) $(CFLAGSEXTRA) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

wgdiplus.$(O): $(W)/wgdiplus.cpp $(WINDEPS)
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(CFLAGSEXTRA) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

wredirect.$(O): $(W)/wredirect.cpp $(WINDEPS)
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(CFLAGSEXTRA) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

wgnuplot.mnu: $(W)/wgnuplot.mnu
	$(CP) $^ $@

wxt_gui.$(O): $(S)/wxterminal/wxt_gui.cpp $(S)/wxterminal/wxt_gui.h
	$(CXX) -c $(CFLAGS) $(CFLAGSEXTRA) $(CXXFLAGS) $(CAIRO_CFLAGS) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

gp_cairo.$(O): $(S)/wxterminal/gp_cairo.c $(S)/wxterminal/gp_cairo.h
	$(CC) -c $(CFLAGS) $(CFLAGSEXTRA) $(CXXFLAGS) $(PANGOCAIRO_CFLAGS) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<

qt_term.$(O): $(S)/qtterminal/qt_term.cpp $(S)/qtterminal/qt_term.h
	$(CXX) -c $(CFLAGS) $(CFLAGSEXTRA) $(CXXFLAGS) -MMD -MT '$*.$$(O)' -MF $*.d -o $@ $<


# Call the Qt resource compiler
qrc_QtGnuplotResource.cpp: $(S)/qtterminal/QtGnuplotResource.qrc
	$(RCC) -name QtGnuplotResource -o $@ $<

# Call the Qt ui compiler
ui_QtGnuplotSettings.h: $(S)/qtterminal/QtGnuplotSettings.ui
	$(UIC) -o $@ $<

# Call the Qt meta-object compiler
moc_QtGnuplotWindow.cpp: $(S)/qtterminal/QtGnuplotWindow.h
	$(MOC) -o $@ $<

moc_QtGnuplotApplication.cpp: $(S)/qtterminal/QtGnuplotApplication.h
	$(MOC) -o $@ $<

moc_QtGnuplotWidget.cpp: $(S)/qtterminal/QtGnuplotWidget.h
	$(MOC) -o $@ $<

moc_QtGnuplotScene.cpp: $(S)/qtterminal/QtGnuplotScene.h
	$(MOC) -o $@ $<

moc_QtGnuplotEvent.cpp: $(S)/qtterminal/QtGnuplotEvent.h
	$(MOC) -o $@ $<
# end (Call the Qt ui compiler)

# Translation files
qtgnuplot_fr.qm: $(S)/qtterminal/po/qtgnuplot_fr.ts
	$(LRELEASE) -qm $@ $<

qtgnuplot_ja.qm: $(S)/qtterminal/po/qtgnuplot_ja.ts
	$(LRELEASE) -qm $@ $<

%.o: $(S)/qtterminal/%.cpp
	$(CXX) -c $(CFLAGS) $(CFLAGSEXTRA) $(CXXFLAGS) -MMD -MT '$*.o' -MF $*.d -o $@ $<
%.o: %.cpp
	$(CXX) -c $(CFLAGS) $(CFLAGSEXTRA) $(CXXFLAGS) -MMD -MT '$*.o' -MF $*.d -o $@ $<

gnuplot_qt.exe: $(QT_FILES_TARGETS) $(GNUPLOTQTOBJS)
	$(LDX) -Wl,--subsystem,windows $(LDFLAGS) -Wl,-Map=gnuplot_qt.map -o $@ $(GNUPLOTQTOBJS) $(QT_LIBS) -lshell32

demo_plugin.dll: $(M)plugin/demo_plugin.c
	$(CC) $(CFLAGS) -I$(M)plugin -shared -o $@ $<

# convert gnuplot.doc to windows/wgnuplot.html
$(BUILDHELPFILE): doc2html.exe $(D)gnuplot.doc $(D)windows/wgnuplot.hhp $(D)windows/wgnuplot.hhk \
                  $(D)windows/wgnuplot.stp $(D)plotstyles.gnu $(CORETERM)
	./doc2html $(D)gnuplot.doc $(D)windows/
	-./$(TARGET) -d -e "winhelp=1; cd '$(D)'" plotstyles.gnu
	-$(HHC) $(D)windows/wgnuplot.hhp

$(HELPFILE): $(BUILDHELPFILE)
	$(CP) $^ $@

htmlhelp.lib: htmlhelp.def
	dlltool -d htmlhelp.def -l htmlhelp.lib

htmlhelp.def:
ifndef MINGW64
	pexports c/Windows/System32/hhctrl.ocx > htmlhelp.def
else
	gendef - /c/Windows/System32/hhctrl.ocx > htmlhelp.def
endif

# Working directory for patched Japanese sources 
JA = ja/
JAD = $(JA)docs/
JADW = $(JAD)windows/
# Random file which receives patches by term-ja.diff to get dependencies right
JASRC = $(JA)term/cairo.trm

$(JASRC): $(D)gnuplot-ja.doc $(D)term-ja.diff $(CORETERM)
	# Create a local copy of relevant sources
	-mkdir $(JA)
	-mkdir $(JA)term
	-mkdir $(JAD)
	-mkdir $(JA)demo
	cp -r $(TOP)/term/* $(JA)term/
	cp -r $(TOP)/docs/* $(JA)docs/
	cp $(M)*.dat $(M)*.png $(M)*.rgb $(JA)demo/
	# Apply patch for Japanese docs
	-rm $(JAD)tex2ja.awk $(JAD)sjisenc.c
	-(cd $(JA) ; patch -p0 -i docs/term-ja.diff)

wgnuplot-ja.chm: $(JASRC) doc2html-ja.exe \
                  $(D)windows/wgnuplot.hhp $(D)windows/wgnuplot.hhk \
                  $(D)windows/wgnuplot.stp $(D)plotstyles.gnu
	# FIXME: some files always end up read-only...
	chmod +w $(JADW)*
	# Create Japanese html source in EUC-JP encoding
	./doc2html-ja $(JAD)gnuplot-ja.doc $(JADW)
	# Change character encoding from EUC-JP to SHIFT-JIS
	-iconv -c -s -f EUC-JP -t SHIFT-JIS $(JADW)wgnuplot.html > $(JADW)wgnuplot-ja.html
	-iconv -c -s -f EUC-JP -t SHIFT-JIS $(JADW)wgnuplot.hhc > $(JADW)wgnuplot-ja.hhc
	cp $(JADW)wgnuplot-ja.html $(JADW)wgnuplot.html
	cp $(JADW)wgnuplot-ja.hhc $(JADW)wgnuplot.hhc
	# Adjust Encoding and Language information
	sed -i -e "s/charset=utf-8/charset=shift-jis/" $(JADW)wgnuplot.html
	sed -i -e "s/charset=utf-8/charset=shift-jis/" $(JADW)wgnuplot.hhc
	sed -i -e "s/Language=0x409 Englisch (USA)/Language=0x411 Japanisch (Japan)/" $(JADW)/wgnuplot.hhp
	# Build help file
	-./$(TARGET) -d -e "winhelp=1; cd '$(JAD)'" plotstyles.gnu
	-$(HHC) $(JADW)wgnuplot.hhp
	cp $(JADW)wgnuplot.chm $(HELPFILEJA)

doc2html.exe: $(D)windows/doc2html.c $(D)termdoc.c $(D)xref.c version.$(O)
	$(LD) $(LDFLAGS) -o $@ -DWINDOWS_NO_GUI $(CFLAGS) -I. -I$(D:/=) -I$(T:/=) $^

doc2html-ja.exe: $(JASRC) $(D)windows/doc2html.c $(D)termdoc.c $(D)xref.c version.$(O)
	$(LD) $(LDFLAGS) -o $@ -DWINDOWS_NO_GUI -DJAPANESE_DOC $(CFLAGS) -I. -I$(JAD) -I$(JA)/term $(D)windows/doc2html.c $(D)termdoc.c $(D)xref.c version.$(O)

doc2ms.exe: $(D)doc2ms.c $(D)termdoc.c $(D)xref.c version.$(O) $(LUA_HELP)
	$(LD) $(LDFLAGS) -o $@ -DWINDOWS_NO_GUI $(CFLAGS) -I. -I$(D:/=) -I$(T:/=) $(D)doc2ms.c $(D)termdoc.c $(D)xref.c version.$(O)

#make binary demo files
# WINDOWS_NO_GUI makes sure that wtext.h does not redefine fread() etc.
$(M)bf_test.exe : bf_test.c 
	$(LD) $(LDFLAGS) $(CFLAGS) -DWINDOWS_NO_GUI -o $@ $^
	(cd $(M) ; ./bf_test.exe )

#
# Create config.h
#
$(OBJS) $(WINOBJS): config.h
config.h: $(TOP)/config/config.mgw
	cp -p $< $@


# generate LUA TeX support files and terminal help
ifdef LUA
gnuplot-lua-tikz.sty: $(T)/lua/gnuplot-tikz.lua
	$(LUA) $< style
$(LUA_HELP): $(T)/lua/gnuplot-tikz.lua
	$(LUA) $< termhelp > $@
endif


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
allterm.h: $(CORETERM) $(LUA_HELP)
	@echo "Building allterm.h"
	@cat $(T)*.trm > allterm.c
	$(CPP) $(CFLAGS) -I$(T) -DTERM_DRIVER_H -DTERM_HELP allterm.c | \
	  sed '/^ *$$/d;/^#/d' > allterm.h
	@rm -f allterm.c
endif
doc2tex.exe: $(D)doc2tex.c $(D)termdoc.c allterm.h $(CORETERM) $(LUA_HELP)
	$(LD) $(LDFLAGS) -o $@ -DWINDOWS_NO_GUI -DALL_TERM_DOC $(CFLAGS) \
	  -I. -I$(D) -I$(T) $(D)doc2tex.c $(D)termdoc.c
else
# Old version: generate documentation with only currently used terminals:
doc2tex.exe: $(D)doc2tex.c $(D)termdoc.c $(LUA_HELP)
	$(LD) $(LDFLAGS) -o $@ -DWINDOWS_NO_GUI $(CFLAGS) -I. -I$(D) -I$(T) $^
endif

gnuplot.tex: $(D)gnuplot.doc doc2tex.exe
	./doc2tex $(D)gnuplot.doc gnuplot.tex

gnuplot-figures.tex: $(D)gnuplot.doc doc2tex.exe
	GNUPLOT_LIB=$(M) ./$(TARGET) -d $(D)plotstyles.gnu
	./doc2tex -figures $(D)gnuplot.doc gnuplot-figures.tex

gnuplot.dvi: gnuplot.tex $(TOP)/VERSION $(D)toc_entr.sty $(D)titlepag.tex
	cp gnuplot.tex gp_tex2.tex
	cp $(TOP)/VERSION .
	cp -p $(D)toc_entr.sty .
	cp -p $(D)titlepag.tex .
	echo "" > pdffigures.tex
	# Call LaTeX three times to get the toc right.
	latex gp_tex2.tex && latex gp_tex2.tex && latex gp_tex2.tex
	mv gp_tex2.dvi gnuplot.dvi
	rm -f gp_tex2.*
gnuplot.ps: gnuplot.dvi
	dvips -o gnuplot.ps gnuplot.dvi

gnuplot.pdf: gnuplot-figures.tex $(TOP)/VERSION $(D)toc_entr.sty $(D)titlepag.tex
	cp gnuplot-figures.tex gp_tex2.tex
	echo "\usepackage{graphicx}" > pdffigures.tex
	echo "\usepackage{picins}" >> pdffigures.tex
	# Call LaTeX three times to get the toc right.
	TEXINPUTS=.:$(TOP):$(D):${TEXINPUTS}: pdflatex gp_tex2.tex
	TEXINPUTS=.:$(TOP):$(D):${TEXINPUTS}: pdflatex gp_tex2.tex
	TEXINPUTS=.:$(TOP):$(D):${TEXINPUTS}: pdflatex gp_tex2.tex
	# now include index
	makeindex gp_tex2
	TEXINPUTS=.:$(TOP):$(D):${TEXINPUTS}: pdflatex gp_tex2.tex
	mv gp_tex2.pdf gnuplot.pdf
	rm -f gp_tex2.*

gnuplot.ms: doc2ms.exe $(D)gnuplot.doc
	./doc2ms $(D)gnuplot.doc gnuplot.ms
	dos2unix gnuplot.ms

grops: $(D)titlepag.ms gnuplot.ms
	sed "s,titlepag\.ms,$(D)titlepag\.ms," gnuplot.ms | \
	groff -s -p -t -e -ms -k -K utf-8 -Tps > gnuplot.ps

gropdf: $(D)titlepag.ms gnuplot.ms
	sed "s,titlepag\.ms,$(D)titlepag\.ms," gnuplot.ms | \
	groff -s -p -t -e -ms -k -K utf-8 -Tpdf > gnuplot.pdf

TUT = $(TOP)/tutorial
tutorial.pdf: $(TUT)/eg1.plt $(TUT)/eg2.plt $(TUT)/eg3.plt $(TUT)/eg4.plt \
	$(TUT)/eg5.plt $(TUT)/eg6.plt $(TUT)/eg7.plt $(TUT)/linepoin.plt \
	$(TUT)/test.plt $(TUT)/test_tikz.plt $(TUT)/tutorial.tex
	GNUPLOT_LIB=$(TUT) gnuplot eg1.plt
	GNUPLOT_LIB=$(TUT) gnuplot eg2.plt
	GNUPLOT_LIB=$(TUT) gnuplot eg3.plt
	GNUPLOT_LIB=$(TUT) gnuplot eg4.plt
	GNUPLOT_LIB=$(TUT) gnuplot eg5.plt
	GNUPLOT_LIB=$(TUT) gnuplot eg6.plt
	GNUPLOT_LIB=$(TUT) GNUPLOT_PS_DIR=$(T)PostScript gnuplot eg7.plt
	GNUPLOT_LIB=$(TUT) gnuplot linepoin.plt
	GNUPLOT_LIB=$(TUT) gnuplot test.plt
	GNUPLOT_LIB=$(TUT) GNUPLOT_LUA_DIR=$(T)lua gnuplot test_tikz.plt
	# run latex twice to get refernces right
	TEXINPUTS=.:$(TUT) latex tutorial.tex
	TEXINPUTS=.:$(TUT) latex tutorial.tex
	dvipdfm tutorial.dvi

gpcard.pdf: $(D)gpcard.tex
	TEXINPUTS=$(TOP):$(D):${TEXINPUTS}: tex gpcard.tex
	dvipdfm gpcard.dvi

ps_symbols.pdf: $(D)psdoc/ps_symbols.gpi
	GNUPLOT_LIB=$(D)psdoc GNUPLOT_PS_DIR=$(T)PostScript ./$(TARGET) -d ps_symbols.gpi
	ps2pdf ps_symbols.ps

ps_fontfile_doc.ps: $(D)psdoc/ps_fontfile_doc.tex
	TEXINPUTS=$(D)psdoc:${TEXINPUTS}: latex ps_fontfile_doc
	TEXINPUTS=$(D)psdoc:${TEXINPUTS}: latex ps_fontfile_doc
	dvips -j0 ps_fontfile_doc

ps_fontfile_doc.pdf: $(D)psdoc/ps_fontfile_doc.tex
	TEXINPUTS=$(D)psdoc:${TEXINPUTS}: pdflatex ps_fontfile_doc
	TEXINPUTS=$(D)psdoc:${TEXINPUTS}: pdflatex ps_fontfile_doc

ps_guide.pdf: $(D)psdoc/ps_guide.ps
	ps2pdf $(D)psdoc/ps_guide.ps ps_guide.pdf

# clean up temporary files
clean:
	$(RM) config.h wgnuplot.map wgnuplot.res
	$(RM) *.$(O) *.po *.o *.co *.d *.map
	$(RM) moc_*.cpp ui_*.h qrc_*.cpp
	$(RM) doc2*.exe $(W)/wgnuplib.res wgnuplib.map wgnuplot.lib
	$(RM) $(M)bf_test.exe demo_plugin.dll allterm.h allterm.c
	$(RM) *.aux *.log *.dvi *.toc
	$(RM) gnuplot.tex gnuplot-figures.tex pdffigures.tex figure_*.pdf
	$(RM) titlepag.tex toc_entr.sty VERSION
	$(RM) eg?.tex eg?.eps linepoin.tex test.tex test_tikz.tex
	$(RM) $(D)windows/wgnuplot.log $(D)windows/wgnuplot.hhc
	$(RM) $(D)windows/*.png $(D)windows/wgnuplot.html
	-$(RM) timestamp.h *.tmp *.par
	-$(RM) -rf ./dist/ ./gnuplot/
	-$(RM) -rf $(JA)

realclean: veryclean
veryclean: clean
	$(RM) wgnuplot.exe wgnuplot_pipes.exe gnuplot.exe pgnuplot.exe
	$(RM) gnuplot_qt.exe qtgnuplot_*.qm
	$(RM) $(HELPFILE) $(HELPFILEJA) wgnuplot.chw wgnuplot.gid wgnuplot.mnu
	$(RM) gnuplot.ps gnuplot.pdf
	$(RM) gpcard.pdf tutorial.pdf
	$(RM) ps_fontfile_doc.pdf ps_fontfile_doc.ps ps_guide.pdf
	$(RM) ps_symbols.ps ps_symbols.pdf
	$(RM) $(M)binary[123] $(M)fit.log $(M)soundfit.par
	$(RM) gnuplot-lua-tikz.sty gnuplot-lua-tikz.tex
	$(RM) gnuplot-lua-tikz-common.tex t-gnuplot-lua-tikz.tex

# now move the whole stuff to its destination
install:
	mkdir -p $(DESTDIR)
# binaries go to /bin
	mkdir -p $(DESTDIR)/bin
	cp -p gnuplot.exe $(DESTDIR)/bin/
	cp -p wgnuplot.exe $(DESTDIR)/bin/
	cp -p wgnuplot_pipes.exe $(DESTDIR)/bin/
	cp -p $(W)/wgnuplot.mnu $(DESTDIR)/bin/
	cp -p $(W)/wgnuplot-ja.mnu $(DESTDIR)/bin/
	cp -p $(HELPFILE) $(DESTDIR)/bin/
ifdef HELPFILEJA
	cp -p $(HELPFILEJA) $(DESTDIR)/bin/
endif
# configuration files
	mkdir -p $(DESTDIR)/$(GNUPLOT_SHARE_DIR)
	cp -p $(TOP)/share/gnuplotrc $(DESTDIR)/$(GNUPLOT_SHARE_DIR)
	cp -p $(TOP)/share/*.gp $(DESTDIR)/$(GNUPLOT_SHARE_DIR)
# terminal support files
	mkdir -p $(DESTDIR)/$(GNUPLOT_PS_DIR)
	cp -p $(TOP)/term/PostScript/*.ps $(DESTDIR)/$(GNUPLOT_PS_DIR)/
	cp -p $(TOP)/term/PostScript/*.txt $(DESTDIR)/$(GNUPLOT_PS_DIR)/
	mkdir -p $(DESTDIR)/$(GNUPLOT_JS_DIR)
	cp -p $(TOP)/term/js/*.* $(DESTDIR)/$(GNUPLOT_JS_DIR)/
	cp -p $(TOP)/term/js/README $(DESTDIR)/$(GNUPLOT_JS_DIR)/
	mkdir -p $(DESTDIR)/$(GNUPLOT_TEX_DIR)/generic/gnuplot
	mkdir -p $(DESTDIR)/$(GNUPLOT_TEX_DIR)/plain/gnuplot
	mkdir -p $(DESTDIR)/$(GNUPLOT_TEX_DIR)/latex/gnuplot
	mkdir -p $(DESTDIR)/$(GNUPLOT_TEX_DIR)/context/gnuplot
	cp -p $(TOP)/share/LaTeX/README $(DESTDIR)/$(GNUPLOT_TEX_DIR)/latex/gnuplot/
	cp -p $(TOP)/share/LaTeX/gnuplot.cfg $(DESTDIR)/$(GNUPLOT_TEX_DIR)/latex/gnuplot/
ifdef QT
	mkdir -p $(DESTDIR)/$(GNUPLOT_QT_DIR)
	cp -p gnuplot_qt.exe $(DESTDIR)/bin/
	cp -p qtgnuplot_*.qm $(DESTDIR)/$(GNUPLOT_QT_DIR)/
endif
ifdef LUA
	mkdir -p $(DESTDIR)/$(GNUPLOT_LUA_DIR)
	cp -p $(TOP)/term/lua/README $(DESTDIR)/$(GNUPLOT_LUA_DIR)/
	cp -p $(TOP)/term/lua/NEWS $(DESTDIR)/$(GNUPLOT_LUA_DIR)/
	cp -p $(TOP)/term/lua/TODO $(DESTDIR)/$(GNUPLOT_LUA_DIR)/
	cp -p $(TOP)/term/lua/gnuplot-tikz.lua $(DESTDIR)/$(GNUPLOT_LUA_DIR)/
	cp -p gnuplot-lua-tikz.sty $(DESTDIR)/$(GNUPLOT_TEX_DIR)/latex/gnuplot/
	cp -p gnuplot-lua-tikz.tex $(DESTDIR)/$(GNUPLOT_TEX_DIR)/plain/gnuplot/
	cp -p t-gnuplot-lua-tikz.tex $(DESTDIR)/$(GNUPLOT_TEX_DIR)/context/gnuplot/
	cp -p gnuplot-lua-tikz-common.tex $(DESTDIR)/$(GNUPLOT_TEX_DIR)/generic/gnuplot/
endif
# README etc
	cp -p $(TOP)/Copyright $(DESTDIR)/
	cp -p $(TOP)/win/Copyright-ja.txt $(DESTDIR)/
	cp -p $(TOP)/README $(DESTDIR)/
	cp -p $(TOP)/NEWS $(DESTDIR)/
	cp -p $(TOP)/ChangeLog $(DESTDIR)/
	cp -p $(TOP)/BUGS $(DESTDIR)/
	cp -p $(TOP)/RELEASE_NOTES $(DESTDIR)/
	cp -p $(TOP)/win/README-Windows.txt $(DESTDIR)/
	cp -p $(TOP)/win/README-Windows-ja.txt $(DESTDIR)/
	-cp -p $(TOP)/win/README-testing.txt $(DESTDIR)/
	-cp -p $(TOP)/win/README-testing-ja.txt $(DESTDIR)/
	-unix2dos $(DESTDIR)/*.txt $(DESTDIR)/README $(DESTDIR)/ChangeLog
	-unix2dos $(DESTDIR)/Copyright $(DESTDIR)/NEWS $(DESTDIR)/BUGS $(DESTDIR)/RELEASE_NOTES
# demo and contrib
	mkdir -p $(DESTDIR)/demo
	-cp -p $(M)* $(DESTDIR)/demo/
	mkdir -p $(DESTDIR)/demo/games
	-cp -p $(M)/games/* $(DESTDIR)/demo/games/
	mkdir -p $(DESTDIR)/contrib/pm3d/
	-cp -p $(TOP)/pm3d/contrib/* $(DESTDIR)/contrib/pm3d/
# docs
	mkdir -p $(DESTDIR)/docs
	-cp -p gnuplot.pdf $(DESTDIR)/docs/
	cp -p $(TOP)/FAQ.pdf $(DESTDIR)/docs/
	-cp -p gpcard.pdf $(DESTDIR)/docs/
	-cp -p tutorial.pdf $(DESTDIR)/docs/
	mkdir -p $(DESTDIR)/docs/psdoc
	cp -p $(D)psdoc/ps_file.doc $(DESTDIR)/docs/psdoc/ps_file_doc.txt
	cp -p $(D)psdoc/ps_guide.ps $(DESTDIR)/docs/psdoc/
	-cp -p ps_guide.pdf $(DESTDIR)/docs/psdoc/
	-cp -p ps_symbols.ps $(DESTDIR)/docs/psdoc/
	-cp -p ps_symbols.pdf $(DESTDIR)/docs/psdoc/
	-cp -p ps_fontfile_doc.pdf $(DESTDIR)/docs/psdoc/
# runtime libraries, configuration files etc.
ifdef EXTRADIST
	cp -pR $(EXTRADIST)/* $(DESTDIR)/
endif

installer:
	$(MAKE) DESTDIR=./dist install
	cp -p $(W)/grpicon.ico ./dist/bin/
	cp -p $(TOP)/win/*.iss ./dist
	sed -e '/^#define MyAppVersionShort/ s/"[^"]*"/"@VERSHORT@"/' \
	    -e '/^#define MyAppVersion/ s/"[^"]*"/"@VER@"/' \
	    -e '/^#define MyAppNumVersion/ s/"[^"]*"/"@VERLONG@"/' \
	    -e '/^#define MyInstallerName/ s/"[^"]*"/"@INSTALLER@"/' \
	    -e "s/@VERSHORT@/$(MAINVERSION)/" \
	    -e "s/@VER@/$(FULLVERSION)/" \
	    -e "s/@VERLONG@/$(LONGVERSION)/" \
	    -e "s/\.alpha/\.0/" \
	    $(TOP)/win/gnuplot.iss > ./dist/gnuplot.iss
ifndef M32
	sed -i \
	    -e "s/@INSTALLER@/gp$(SHORTVERSION)-win64-setup/" \
	    -e 's/^\(ArchitecturesAllowed\)=/\1=x64/g' \
	    -e 's/^\(ArchitecturesInstallIn64BitMode\)=/\1=x64/g' \
	    ./dist/gnuplot.iss
else
	sed -i \
	    -e "s/@INSTALLER@/gp$(SHORTVERSION)-win32-setup/" \
	    ./dist/gnuplot.iss
endif
	(cd ./dist; $(ISCC) gnuplot.iss)
ifndef M32
ifdef RELEASE
	mv ./dist/gp*-setup.exe gp$(SHORTVERSION)-win64-mingw.exe
else
	mv ./dist/gp*-setup.exe gp$(SHORTVERSION)-`date +%Y%m%d`-win64-mingw.exe
endif
else
ifdef RELEASE
	mv ./dist/gp*-setup.exe gp$(SHORTVERSION)-win32-mingw.exe
else
	mv ./dist/gp*-setup.exe gp$(SHORTVERSION)-`date +%Y%m%d`-win32-mingw.exe
endif
endif

zip:
	$(MAKE) DESTDIR=./gnuplot install
ifndef M32
ifdef RELEASE
	zip -mro9 gp$(SHORTVERSION)-win64-mingw.zip gnuplot
else
	zip -mro9 gp$(SHORTVERSION)-`date +%Y%m%d`-win64-mingw.zip gnuplot
endif
else
ifdef RELEASE
	zip -mro9 gp$(SHORTVERSION)-win32-mingw.zip gnuplot
else
	zip -mro9 gp$(SHORTVERSION)-`date +%Y%m%d`-win32-mingw.zip gnuplot
endif
endif

# rebuild makefile.all
maint: 
	-$(RM) $(S)/makefile.all $(S)/makefile.awc
	$(MAKE) -C $(S) srcdir=. top_srcdir=.. -f Makefile.maint makefile.all makefile.awc
