#
# $Id: Makefile,v 1.25 2014/08/18 03:49:06 sfeam Exp $
#
# GNUPLOT Makefile Microsoft Visual C++ 2005 or newer

# John Bollinger bbands@yahoo.com, 4 March 2002:
#   Added png support with zlib and libpng as subs of .\src
#     libpng and zlib can be downloaded from ftp://ftp.uu.net/graphics/png
#   Added pdf support with pdflib as a sub of .\src
#     pdflib can be downloaded from http://www.pdflib.com
#   Added MSC multi-thread support for compatibility with pdflib
# Bastian Maerkisch bmaerkisch@web.de:
#   Added support for binary version of the GD library (bgd.dll)
#     gdlib can be downloaded from http://www.libgd.org/
#     Development of libgd seems to have stalled:
#     Latest binary build can be found at http://www.libgd.org/releases/gd-latest-win32.zip
#     As of March 2011 this is version 2.0.34.
#   Removed zlib and libpng again as they're already included in bgd.dll
# Hans-Bernhard Broeker:
#   updated to current version of MSVC
#   undid silly name change for pgnuplot.exe, call console version gnuplot.exe instead
#   removed remainder of wgnuplot_pipes.exe support
#   isolated more configuration-dependent parts (gd, pdf, wx) to lines of their own,
#     so they can be turned on/off more easily.
# Bastian Maerkisch bmaerkisch@web.de:
#   Out of tree version. Moved to config\msvc
#   See ChangeLog for further changes.

#
# To compile with Visual C:
#	Optionally download gdlib to config\msvc\gdwin32
#	Optionally download pdflib to config\msvc\pdflib and compile it if you choose the source
#
#	Start the Microsoft Visual C++ command shell (e.g. via link installed setup)
#	change to the gnuplot\config\msvc directory
#   Edit the Makefile to match your setup. (If you don't have the optional libraries,
#   you will probably have to disable some parts.)
#	Now run:
#	  nmake
#

# Compilation options, disable by setting to zero
# Is libgd available? (required by terminal png/jpeg/gif)?
GDLIB = 0
# Is pdflib available? (required by terminal pdf)
PDFLIB = 0
# Build wxt terminal? (requires GTK, wxWidgets, cairo and pango libraries)
# See http://wiki.wxwidgets.org/Microsoft_Visual_C%2B%2B_Guide
#     http://wiki.wxwidgets.org/Visual_Studio_2008_%289.0%29
WXT = 0
# Build Cairo Terminals (cairolatex, epslatex, pdflatex)
CAIRO = 1
# Build qt terminal?
QT = 0
# Set to one if you have Qt5 (instead of Qt4)
QT5 = 1
# Build the lua/tikz terminal?
LUA = 0
# Build caca terminal? Requires libcaca
# Requires libcaca version 0.99.beta15 or newer. A post-beta18
# svn version is recommended since it contains an improved
# win32 backend.
CACA = 0

# Create debug build?
DEBUG = 0

# Location of additional libraries
#GDDIR = gdwin32
#LUADIR = lua
PDFDIR = pdflib
#QTDIR = C:\\Qt\\4.8.4
QTDIR = d:\Source\Qt-5.2.1\5.2.1\msvc2012
#CAIRODIR = cairo-pango
#CACADIR = libcaca
CAIRODIR = d:\msvc-libs
LIBDIR =  d:\msvc-libs


# installation directory
DESTDIR = "$(PROGRAMFILES)\gnuplot"

TOP = ..\\..

# directory for PostScript prolog and associated files
GNUPLOT_PS_DIR = share\\PostScript

# Similarly for javascript files for the canvas terminal:
GNUPLOT_JS_DIR = share\\js

# Similarly for scripts used by the lua terminal
GNUPLOT_LUA_DIR = share\\lua

# directory for Qt translation files
GNUPLOT_QT_DIR = share\\qt

# gnuplot will look for gnuplotrc here:
GNUPLOT_SHARE_DIR = share

#CC = cl
LD = link

# compiler flags
# /I  means extra include directory
# /Fm means produce link map
# /Od means suppress optimizations (esp. for debug)
# /O2 mans optimize for speed
# /Zi means prepare for codeview
# /MD means include multi-thread support (DLL)
# /GR means Enable Run-Time Type Information
!IF "$(DEBUG)" == "1"
CBASEFLAGS = /EHsc /GR /MD /Od /nologo /Zi /DDEBUG
!ELSE
CBASEFLAGS = /EHsc /GR /MD /O2 /nologo
!ENDIF
CFLAGS = $(CBASEFLAGS) /I. /I$(TOP) /I$(S) /I$(T) /D_Windows /DWIN32 /DHAVE_CONFIG_H /D__MSC__ $(OPTIONS)
CFLAGS = $(CFLAGS) /DGNUPLOT_SHARE_DIR=\"$(GNUPLOT_SHARE_DIR)\"
WIN_CFLAGS = /DUSE_FAKEPIPES
CONSOLE_CFLAGS = /DWGP_CONSOLE /DCONSOLE_SWITCH_CP /DPIPES
OPTIONS = /DUSE_MOUSE
OPTIONS = $(OPTIONS) /DGNUPLOT_HISTORY

EXTRA_TARGETS =

# flags for terminals
# see other terminal defines in term.h
TERMFLAGS = /I$(T) /DGNUPLOT_PS_DIR=\"$(GNUPLOT_PS_DIR)\" /DGNUPLOT_JS_DIR=\"$(GNUPLOT_JS_DIR)\"

# linker flags, append paths for external libs later
LDFLAGS = /nologo /MACHINE:$(MACHINE) /MANIFEST
LDFLAGS = $(LDFLAGS) /MANIFESTDEPENDENCY:"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
!IF "$(DEBUG)" == "1"
LDFLAGS = $(LDFLAGS) /DEBUG
!ENDIF
COMMONLIBS = kernel32.lib user32.lib gdi32.lib winspool.lib \
             comdlg32.lib comctl32.lib advapi32.lib shell32.lib \
             msimg32.lib gdiplus.lib
CONSOLELIBS = ole32.lib oleaut32.lib comctl32.lib

!IFDEF LIBDIR
OPTIONS = $(OPTIONS) /I$(LIBDIR)\include
LDFLAGS = $(LDFLAGS) /libpath:$(LIBDIR)\lib
!ENDIF

!IF "$(GDLIB)" == "1"
OPTIONS = $(OPTIONS) /DHAVE_LIBGD /DHAVE_GD_H /DHAVE_GD_GIF /DGIF_ANIMATION /DHAVE_GD_PNG /DHAVE_GD_JPEG /DHAVE_GD_TTF
!IFDEF GDDIR
OPTIONS = $(OPTIONS) /I$(GDDIR)\include
LDFLAGS = $(LDFLAGS) /libpath:$(GDDIR)\lib
!ENDIF
!ENDIF

!IF "$(PDFLIB)" == "1"
OPTIONS = $(OPTIONS) /DHAVE_LIBPDF
!IFDEF PDFDIR
LDFLAGS = $(LDFLAGS) /libpath:$(PDFDIR)\pdflib
TERMFLAGS = $(TERMFLAGS) /I$(PDFDIR)\pdflib
!ENDIF
!ENDIF

!IF ("$(CAIRO)" == "1") || ("$(WXT)" == "1")
CAIROLIBS = glib-2.0.lib gobject-2.0.lib gmodule-2.0.lib \
	cairo.lib pango-1.0.lib pangocairo-1.0.lib
CFLAGS = $(CFLAGS) /DHAVE_CAIROPDF
TERMFLAGS = $(TERMFLAGS) /DHAVE_CAIROEPS
!IFDEF CAIRODIR
CFLAGS = $(CFLAGS) /I$(CAIRODIR)\include\glib-2.0 /I$(CAIRODIR)\lib\glib-2.0\include /I$(CAIRODIR)\include\pango-1.0 /I$(CAIRODIR)\include\msvc
!ENDIF
!ENDIF

!IF "$(WXT)" == "1"
WXTLIBS = wxmsw28.lib wxtiff.lib
!ENDIF

!IF "$(QT)" == "1"
!IF "$(DEBUG)" == "1"
!IF "$(QT5)" == "1"
QTLIBS = Qt5Cored.lib Qt5Guid.lib Qt5Networkd.lib Qt5Svgd.lib Qt5Widgetsd.lib Qt5PrintSupportd.lib qtmaind.lib
!ELSE
QTLIBS = QtCored4.lib QtGuid4.lib QtNetworkd4.lib QtSvgd4.lib
!ENDIF
!ELSE
!IF "$(QT5)" == "1"
QTLIBS = Qt5Core.lib Qt5Gui.lib Qt5Network.lib Qt5Svg.lib Qt5Widgets.lib Qt5PrintSupport.lib qtmain.lib
!ELSE
QTLIBS = QtCore4.lib QtGui4.lib QtNetwork4.lib QtSvg4.lib
!ENDIF
!ENDIF
CFLAGS = $(CFLAGS) /I$(QTDIR)\include /I$(QTDIR)\include\QtCore /I$(QTDIR)\include\QtGui /I$(QTDIR)\include\QtWidgets /I$(QTDIR)\include\QtNetwork /I$(QTDIR)\include\QtSvg /DQTGNUPLOT_DATA_DIR=\"$(GNUPLOT_QT_DIR)\"
LDFLAGS = $(LDFLAGS) /libpath:$(QTDIR)\lib
CFLAGS = $(CFLAGS) -DQTTERM -DQT_NO_OPENGL
MOC=$(QTDIR)\\bin\\moc
UIC=$(QTDIR)\\bin\\uic
RCC=$(QTDIR)\\bin\\rcc
EXTRA_TARGETS = $(EXTRA_TARGETS) gnuplot_qt.exe
!ENDIF

!IF "$(LUA)" == "1"
OPTIONS = $(OPTIONS) /DHAVE_LUA
!IFDEF LUADIR
OPTIONS = $(OPTIONS) /I$(LUADIR)/include
LDFLAGS = $(LDFLAGS) /libpath:$(LUADIR)
!ENDIF
TERMFLAGS = $(TERMFLAGS) /DGNUPLOT_LUA_DIR=\"$(GNUPLOT_LUA_DIR)\"
LUALIBS = lua52.lib
!ENDIF

!IF "$(CACA)" == "1"
OPTIONS = $(OPTIONS) /DHAVE_LIBCACA 
!IFDEF CACADIR
OPTIONS = $(OPTIONS) /I$(CACADIR)
LDFLAGS = $(LDFLAGS) /libpath:$(CACADIR)
!ENDIF
CACALIBS = libcaca.lib
!ENDIF

HELPFILE = wgnuplot.chm
HHWPATH=$(PROGRAMFILES)\HTML Help Workshop\\

!IF "$(PROCESSOR_ARCHITECTURE)" == "ALPHA"
MACHINE = $(PROCESSOR_ARCHITECTURE)
!ELSE
MACHINE = IX86
!ENDIF

# macros for makefile.all

O=obj
S=$(TOP)\\src
W=$(S)\\win\\
WX=$(S)\\wxterminal\\
Q=$(S)\\qtterminal\\
T=$(TOP)\\term\\
D=$(TOP)\\docs\\
M=$(TOP)\\demo\\

default: wgnuplot.exe $(HELPFILE) wgnuplot.mnu $(M)bf_test.exe gnuplot.exe demo_plugin.dll $(EXTRA_TARGETS)

!INCLUDE $(S)\\makefile.all

OBJS = $(COREOBJS) version.obj

!IF "$(CAIRO)" == "1"
OBJS = $(OBJS) gp_cairo.$(O) gp_cairo_helpers.$(O)
!ENDIF

!IF "$(QT)" == "1"
OBJS = $(OBJS) qt_term.$(O)
GNUPLOTQTOBJS = gnuplot_qt.$(O) QtGnuplotWindow.$(O) \
QtGnuplotApplication.$(O) QtGnuplotWidget.$(O) \
QtGnuplotScene.$(O) QtGnuplotItems.$(O) \
QtGnuplotEvent.$(O) \
moc_QtGnuplotWindow.$(O) moc_QtGnuplotApplication.$(O) \
moc_QtGnuplotWidget.$(O) moc_QtGnuplotScene.$(O) \
moc_QtGnuplotEvent.$(O) qrc_QtGnuplotResource.$(O)

# Call the Qt resource compiler
qrc_QtGnuplotResource.cpp: $(Q)QtGnuplotResource.qrc
	$(RCC) -name QtGnuplotResource -o $@ $**

# Call the Qt ui compiler
ui_QtGnuplotSettings.h: $(Q)QtGnuplotSettings.ui
	$(UIC) -o $@ $**

moc_QtGnuplotWindow.cpp: $(Q)QtGnuplotWindow.h
	$(MOC) -o $@ $**

moc_QtGnuplotApplication.cpp: $(Q)QtGnuplotApplication.h
	$(MOC) -o $@ $**

moc_QtGnuplotWidget.cpp: $(Q)QtGnuplotWidget.h
	$(MOC) -o $@ $**

moc_QtGnuplotScene.cpp: $(Q)QtGnuplotScene.h
	$(MOC) -o $@ $**

moc_QtGnuplotEvent.cpp: $(Q)QtGnuplotEvent.h
	$(MOC) -o $@ $**

gnuplot_qt.exe: ui_QtGnuplotSettings.h $(GNUPLOTQTOBJS)
	$(LD) /entry:mainCRTStartup /subsystem:windows $(LDFLAGS) /map:gnuplot_qt.map /out:$@ $(GNUPLOTQTOBJS) $(QTLIBS) shell32.lib
!ENDIF

WINOBJS = winmain.obj wgnuplib.obj wgraph.obj wprinter.obj wtext.obj \
	screenbuf.obj wpause.obj wmenu.obj gpexecute.obj wgdiplus.obj \
	wredirect.obj

ALL_CONSOLE_OBJS = $(OBJS:.obj=.cobj) $(WINOBJS:.obj=.cobj)


# default rules
.cpp.obj:
	$(CC) /c $(CFLAGS) $(WIN_CFLAGS) $*.cpp /Fo$@

.cpp.cobj:
	$(CC) /c $(CFLAGS) $(CONSOLE_CFLAGS) $*.cpp /Fo$@

{$(S)}.c.obj:
	$(CC) /c $(CFLAGS) $(WIN_CFLAGS) $(S)\\$*.c

{$(S)}.c.cobj:
	$(CC) /c $(CFLAGS) $(CONSOLE_CFLAGS) $(S)\\$*.c /Fo$@

{$(W)}.c.obj:
	$(CC) /c $(CFLAGS) $(WIN_CFLAGS) $(W)$*.c /Fo$@

{$(W)}.c.cobj:
	$(CC) /c $(CFLAGS) $(CONSOLE_CFLAGS) $(W)$*.c /Fo$@

{$(W)}.cpp.obj:
	$(CC) /c $(CFLAGS) $(WIN_CFLAGS) $(W)$*.cpp /Fo$@

{$(W)}.cpp.cobj:
	$(CC) /c $(CFLAGS) $(CONSOLE_CFLAGS) $(W)$*.cpp /Fo$@

{$(WX)}.c.obj:
	$(CC) /c $(CFLAGS) $(WIN_CFLAGS) $(WX)$*.c /Fo$@

{$(WX)}.c.cobj:
	$(CC) /c $(CFLAGS) $(CONSOLE_CFLAGS) $(WX)$*.c /Fo$@

{$(Q)}.cpp.obj:
	$(CC) /c $(CFLAGS) $(WIN_CFLAGS) $(Q)$*.cpp /Fo$@

{$(Q)}.cpp.cobj:
	$(CC) /c $(CFLAGS) $(CONSOLE_CFLAGS) $(Q)$*.cpp /Fo$@

{$(S)}.c{$(M)}.exe:
	$(CC) /c $(CFLAGS) $(W)$*.c /Fo$@


$(OBJS): config.h

config.h: $(TOP)\config\config.nt
	copy $(TOP)\config\config.nt config.h

wgnuplot.exe: $(OBJS) $(WINOBJS) wgnuplot.res linkopt-w.msw
	$(LD) /subsystem:windows $(LDFLAGS) /map:wgnuplot.map /out:$@ @linkopt-w.msw
!IFDEF GDDIR
	copy $(GDDIR)\bin\*.dll .
!ENDIF
!IFDEF LUADIR
	copy $(LUADIR)\*.dll .
!ENDIF
!IFDEF CACADIR
	copy $(CACADIR)\*.dll .
!ENDIF

gnuplot.exe: $(ALL_CONSOLE_OBJS) wgnuplot.res linkopt-c.msw
	$(LD) /subsystem:console $(LDFLAGS) /map:wgnuplot.map /out:$@ @linkopt-c.msw
!IFDEF GDDIR
	copy $(GDDIR)\bin\*.dll .
!ENDIF
!IFDEF LUADIR
	copy $(LUADIR)\*.dll .
!ENDIF
!IFDEF CACADIR
	copy $(CACADIR)\*.dll .
!ENDIF

pgnuplot.exe: $(W)pgnuplot.c
	$(CC) $(CBASEFLAGS) /I. /I$(TOP) /I$(S) /DWINDOWS_NO_GUI $(W)pgnuplot.c /Fe$@ /link version.obj user32.lib

linkopt-w.msw: Makefile
	echo $(OBJS) > linkopt-w.msw
	echo $(WINOBJS) >> linkopt-w.msw
	echo $(COMMONLIBS) >> linkopt-w.msw
	echo wgnuplot.res >> linkopt-w.msw
	echo htmlhelp.lib >> linkopt-w.msw
!IF "$(GDLIB)" == "1"
	echo bgd.lib >> linkopt-w.msw
!ENDIF
!IF "$(PDFLIB)" == "1"
	echo pdflib.lib >> linkopt-w.msw
!ENDIF
!IF "$(CAIRO)" == "1"
	echo $(CAIROLIBS) >> linkopt-w.msw
!ENDIF
!IF "$(WXT)" == "1"
	echo $(WXTLIBS) >> linkopt-w.msw
!ENDIF
!IF "$(QT)" == "1"
	echo $(QTLIBS) >> linkopt-w.msw
!ENDIF
!IF "$(LUA)" == "1"
	echo $(LUALIBS) >> linkopt-w.msw
!ENDIF
!IF "$(CACA)" == "1"
	echo $(CACALIBS) >> linkopt-w.msw
!ENDIF

linkopt-c.msw: Makefile
	echo $(ALL_CONSOLE_OBJS) > linkopt-c.msw
	echo $(COMMONLIBS) >> linkopt-c.msw
	echo $(CONSOLELIBS) >> linkopt-c.msw
	echo wgnuplot.res >> linkopt-c.msw
	echo htmlhelp.lib >> linkopt-c.msw
!IF "$(GDLIB)" == "1"
	echo bgd.lib >> linkopt-c.msw
!ENDIF
!IF "$(PDFLIB)" == "1"
	echo pdflib.lib >> linkopt-c.msw
!ENDIF
!IF "$(CAIRO)" == "1"
	echo $(CAIROLIBS) >> linkopt-c.msw
!ENDIF
!IF "$(WXT)" == "1"
	echo $(WXTLIBS) >> linkopt-c.msw
!ENDIF
!IF "$(QT)" == "1"
	echo $(QTLIBS) >> linkopt-c.msw
!ENDIF
!IF "$(LUA)" == "1"
	echo $(LUALIBS) >> linkopt-c.msw
!ENDIF
!IF "$(CACA)" == "1"
	echo $(CACALIBS) >> linkopt-c.msw
!ENDIF

# rules

term.obj: $(S)\term.c $(S)\term.h $(S)\plot.h $(S)\setshow.h $(S)\bitmap.h $(CORETERM)
	$(CC) /c $(CFLAGS) $(WIN_CFLAGS) $(TERMFLAGS) $(S)\$(*B).c /Fo$@

term.cobj: $(S)\term.c $(S)\term.h $(S)\plot.h $(S)\setshow.h $(S)\bitmap.h $(CORETERM)
	$(CC) /c $(CFLAGS) $(CONSOLE_CFLAGS) $(TERMFLAGS) $(S)\$(*B).c /Fo$@

winmain.obj: $(W)winmain.c $(W)wgnuplib.h $(W)wtext.h $(S)\plot.h
	$(CC) /c $(CFLAGS) $(WIN_CFLAGS) $(W)$(*B).c /Fo$@

winmain.cobj: $(W)winmain.c $(W)wgnuplib.h $(W)wtext.h $(S)\plot.h
	$(CC) /c $(CFLAGS) $(CONSOLE_CFLAGS) $(W)winmain.c /Fo$@


# resources
wgnuplot.mnu: $(W)wgnuplot.mnu
	copy $(W)wgnuplot.mnu wgnuplot.mnu

wgnuplot.res :  $(W)wgnuplot.rc $(W)wgnuplib.rc $(W)wresourc.h $(W)grpicon.ico $(W)texticon.ico
	$(RC) /l 0x409 /fowgnuplot.res /I $(W) /d "NDEBUG" /d WIN32 /d NO_MANIFEST $(W)wgnuplot.rc


# build windows help file
$(HELPFILE): doc2html.exe $(D)gnuplot.doc gnuplot.exe
	doc2html $(D)gnuplot.doc $(D)windows\\
	cd $(D)
	-"..\\config\\msvc\\gnuplot" -e "winhelp=1" plotstyles.gnu
	-"$(HHWPATH)hhc" windows\wgnuplot.hhp
	cd ..\\config\\msvc
	if exist $(D)windows\wgnuplot.chm copy $(D)windows\wgnuplot.chm .

doc2html.exe: $(D)windows\doc2html.c $(D)termdoc.c $(D)xref.c version.obj $(CORETERM)
	$(CC) $(CFLAGS) /F 5000 /W1 /I. /DWINDOWS_NO_GUI /I$(D) /I$(T) -Fedoc2html.exe $(D)windows\doc2html.c $(D)termdoc.c $(D)xref.c version.obj


# make binary demo files
# WINDOWS_NO_GUI makes sure that wtext.h does not redefine fread() etc.
$(M)bf_test.exe : $(S)\\bf_test.c
	$(CC) $(CFLAGS) /F 5000 /W1 /DWINDOWS_NO_GUI /Fe$(M)bf_test.exe $(S)\\bf_test.c
	cd $(M)
	bf_test
	cd ..\\config\\msvc

demo_plugin.dll: $(M)plugin\\demo_plugin.c
	$(CC) $(CFLAGS) /LD /D_USRDLL /D_WINDLL $(M)plugin\\$*.c /link /out:$@

# clean up temporary files
clean:
	if exist *.obj del *.obj
	if exist *.cobj del *.cobj
	if exist *.map del *.map
	if exist moc_*.cpp del moc_*.cpp
	if exist qrc_*.cpp del qrc_*.cpp
	if exist ui_*.h del ui_*.h
	if exist wgnuplot.res del wgnuplot.res
	if exist wgnuplot.chw del wgnuplot.chw
	if exist $(M)bf_test.exe del $(M)bf_test.exe
	if exist linkopt-w.msw del linkopt-w.msw
	if exist linkopt-c.msw del linkopt-c.msw
	if exist doc2html.exe del doc2html.exe
	if exist gnuplot rmdir /s /q gnuplot
	if exist *.tmp del *.tmp

veryclean: clean
	if exist wgnuplot.exe del wgnuplot.exe
	if exist wgnuplot.chm del wgnuplot.chm
	if exist wgnuplot.hlp del wgnuplot.hlp
	if exist wgnuplot.mnu del wgnuplot.mnu
	if exist pgnuplot.exe del pgnuplot.exe
	if exist gnuplot.exe del gnuplot.exe
	if exist gnuplot_qt.exe del gnuplot_qt.exe
	if exist *.manifest del *.manifest
	if exist $(M)binary1 del $(M)binary1
	if exist $(M)binary2 del $(M)binary2
	if exist $(M)binary3 del $(M)binary3
	if exist demo_plugin.dll del demo_plugin.*
	if exist $(M)fit.log del $(M)fit.log
	if exist $(M)soundfit.par del $(M)soundfit.par
	if exist config.h del config.h

install:
	if not exist $(DESTDIR) mkdir $(DESTDIR)
	if not exist $(DESTDIR)\bin mkdir $(DESTDIR)\bin
	copy /Y wgnuplot.exe $(DESTDIR)\bin
	copy /Y gnuplot.exe $(DESTDIR)\bin
	copy /Y gnuplot_qt.exe $(DESTDIR)\bin
	copy /Y *gnuplot.exe.manifest $(DESTDIR)\bin
	copy /Y $(W)wgnuplot.mnu $(DESTDIR)\bin
	copy /Y $(W)wgnuplot-ja.mnu $(DESTDIR)\bin
	if exist wgnuplot.chm copy /Y wgnuplot.chm $(DESTDIR)\bin
	if exist wgnuplot-ja.chm copy /Y wgnuplot-ja.chm $(DESTDIR)\bin
	if exist wgnuplot.hlp copy /Y wgnuplot.hlp $(DESTDIR)\bin
	copy /Y *.dll $(DESTDIR)\bin
	if exist platform\ if not exist $(DESTDIR)\bin\platforms mkdir $(DESTDIR)\bin\platforms
	if exist platform\ xcopy /Y /S platforms $(DESTDIR)\bin\platforms
	if not exist $(DESTDIR)\$(GNUPLOT_PS_DIR) mkdir $(DESTDIR)\$(GNUPLOT_PS_DIR)
	xcopy /Y $(T)PostScript\*.ps $(DESTDIR)\$(GNUPLOT_PS_DIR)
	if not exist $(DESTDIR)\$(GNUPLOT_JS_DIR) mkdir $(DESTDIR)\$(GNUPLOT_JS_DIR)
	xcopy /Y $(T)js\*.* $(DESTDIR)\$(GNUPLOT_JS_DIR)
	if not exist $(DESTDIR)\$(GNUPLOT_LUA_DIR) mkdir $(DESTDIR)\$(GNUPLOT_LUA_DIR)
	xcopy /Y $(T)lua\*.* $(DESTDIR)\$(GNUPLOT_LUA_DIR)
	if not exist $(DESTDIR)\$(GNUPLOT_SHARE_DIR) mkdir $(DESTDIR)\$(GNUPLOT_SHARE_DIR)
	copy /Y $(TOP)\share\gnuplotrc $(DESTDIR)\$(GNUPLOT_SHARE_DIR)
	copy /Y $(TOP)\share\*.gp $(DESTDIR)\$(GNUPLOT_SHARE_DIR)
	if not exist $(DESTDIR)\share\LaTeX mkdir $(DESTDIR)\share\LaTeX
	copy /Y $(TOP)\share\LaTeX\README $(DESTDIR)\share\LaTeX
	copy /Y $(TOP)\share\LaTeX\gnuplot.cfg $(DESTDIR)\share\LaTeX
	if not exist $(DESTDIR)\docs mkdir $(DESTDIR)\docs
	for %f in (NEWS) do copy $(TOP)\%f $(DESTDIR)\%f
	for %f in (BUGS README Copyright ChangeLog FAQ.pdf) do copy $(TOP)\%f $(DESTDIR)\docs\%f
	if not exist $(DESTDIR)\license mkdir $(DESTDIR)\license
	for %f in (Copyright) do copy $(TOP)\%f $(DESTDIR)\license\%f
	if exist $(TOP)\win\README-Windows.txt copy /Y $(TOP)\win\README-Windows.txt $(DESTDIR)
	if exist $(TOP)\win\README-Windows-ja.txt copy /Y $(TOP)\win\README-Windows-ja.txt $(DESTDIR)
	if not exist $(DESTDIR)\demo mkdir $(DESTDIR)\demo
	xcopy /Y $(TOP)\demo $(DESTDIR)\demo
	if not exist $(DESTDIR)\demo\games mkdir $(DESTDIR)\demo\games
	xcopy /Y $(TOP)\demo\games $(DESTDIR)\demo\games
	if not exist $(DESTDIR)\contrib\pm3d mkdir $(DESTDIR)\contrib\pm3d
	xcopy /Y $(TOP)\pm3d\contrib\*.* $(DESTDIR)\contrib\pm3d

zip:
	$(MAKE) DESTDIR=.\gnuplot install
	zip -mro9 gp500-msvc-win32.zip gnuplot
