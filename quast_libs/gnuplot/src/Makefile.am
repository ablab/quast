## Process this file with automake to produce Makefile.in -*-Makefile-*-
AUTOMAKE_OPTIONS = foreign subdir-objects

# in the spirit of automake ...
pkglibexecdir = $(libexecdir)/@PACKAGE@/@VERSION_MAJOR@

# Where to send email about bugs and comments (locally)
EMAIL = gnuplot-bugs@lists.sourceforge.net

# Default help file location
HELPFILE=@GIHDIR@/gnuplot.gih

# Location of X11 app defaults
XAPPLRESDIR=@X11_APPDEFAULTS_DIR@

# Default X11 driver location
X11_DRIVER_DIR=$(pkglibexecdir)

# Default Qt driver location
QT_DRIVER_DIR=$(pkglibexecdir)

# Default lua driver location
GNUPLOT_LUA_DIR=$(pkgdatadir)/$(VERSION_MAJOR)/lua

# Default javascript location
GNUPLOT_JS_DIR=$(pkgdatadir)/$(VERSION_MAJOR)/js

# Default Postscript prologues location
GNUPLOT_PS_DIR=$(pkgdatadir)/$(VERSION_MAJOR)/PostScript

# System-wide initialization file
GNUPLOT_SHARE_DIR=$(pkgdatadir)/$(VERSION_MAJOR)

# There is probably no need to edit anything below this line

bin_PROGRAMS = gnuplot
noinst_PROGRAMS = bf_test
binonly: $(bin_PROGRAMS)

transform = @program_transform_name@
GNUPLOT_X11 = `echo gnuplot_x11 | sed '$(transform)'`$(EXEEXT)

AM_CPPFLAGS = -I../term -I$(top_srcdir)/term -DBINDIR=\"$(bindir)\" -DX11_DRIVER_DIR=\"$(X11_DRIVER_DIR)\" -DQT_DRIVER_DIR=\"$(QT_DRIVER_DIR)\" -DGNUPLOT_SHARE_DIR=\"$(GNUPLOT_SHARE_DIR)\" -DGNUPLOT_PS_DIR=\"$(GNUPLOT_PS_DIR)\" -DGNUPLOT_JS_DIR=\"$(GNUPLOT_JS_DIR)\" -DGNUPLOT_LUA_DIR=\"$(GNUPLOT_LUA_DIR)\"  -DCONTACT=\"$(EMAIL)\" -DHELPFILE=\"$(HELPFILE)\" -DGNUPLOT_X11=\"$(GNUPLOT_X11)\" -DXAPPLRESDIR=\"$(XAPPLRESDIR)\"

gnuplot_SOURCES = alloc.c alloc.h axis.c axis.h breaders.c breaders.h bitmap.h \
boundary.c boundary.h color.c color.h command.c command.h contour.c contour.h \
datablock.c datablock.h datafile.c datafile.h dynarray.c dynarray.h \
eval.c eval.h external.c external.h fit.c fit.h gadgets.c gadgets.h getcolor.c getcolor.h \
gp_hist.h gp_time.h gp_types.h gplt_x11.h graph3d.c graph3d.h graphics.c graphics.h \
help.c help.h hidden3d.c hidden3d.h history.c internal.c internal.h \
interpol.c interpol.h libcerf.c libcerf.h \
matrix.c matrix.h misc.c misc.h mouse.c mouse.h mousecmn.h \
multiplot.c multiplot.h national.h parse.c parse.h plot.c plot.h plot2d.c plot2d.h \
plot3d.c plot3d.h pm3d.c pm3d.h readline.c readline.h save.c \
save.h scanner.c scanner.h set.c setshow.h show.c specfun.c specfun.h \
standard.c standard.h stats.h stats.c stdfn.c stdfn.h syscfg.h tables.c tables.h \
tabulate.c tabulate.h \
template.h term_api.h term.c term.h time.c unset.c util.c util.h \
util3d.c util3d.h variable.c variable.h version.c version.h

gnuplot_LDADD = $(TERMLIBS) $(TERMXLIBS) $(WX_LIBS) $(QT_LIBS)

pkglibexec_PROGRAMS = 

if BUILD_GNUPLOT_X11
pkglibexec_PROGRAMS += gnuplot_x11
gnuplot_x11_SOURCES = gplt_x11.c gplt_x11.h gpexecute.c getcolor.c getcolor.h gpexecute.h mousecmn.h version.c version.h
gnuplot_x11_CPPFLAGS = -DGPLT_X11_MODE $(AM_CPPFLAGS)
XLIBS = @LIBRARIES_FOR_X@
gnuplot_x11_LDADD = $(XLIBS)
endif

bf_test_SOURCES = bf_test.c
bf_test_LIBS = -lm

if BUILD_SRC_BEOS_SUBDIR
d_beos = beos
endif

SUBDIRS = $(d_beos) wxterminal qtterminal

EXTRA_gnuplot_SOURCES = strftime.c vms.c

EXTRA_DIST = GNUmakefile Makefile.maint NeXT OpenStep \
genopt.com gnuplot.opt x11.opt linkopt.vms \
makefile.all makefile.awc os2 win \
$(EXTRA_gnuplot_SOURCES)

# Hercules and original pc graphics driver code
# corgraph.asm header.mac hrcgraph.asm pcgraph.asm lineproc.mac

if BUILD_WXWIDGETS
gnuplot_SOURCES += wxterminal/wxt_gui.cpp
endif

if BUILD_GPCAIRO
gnuplot_SOURCES += wxterminal/gp_cairo.c wxterminal/gp_cairo_helpers.c
endif

if BUILD_BITMAP
gnuplot_SOURCES += bitmap.c
endif

DISTCLEANFILES = timestamp.h
BUILT_SOURCES = timestamp.h
timestamp.h: $(top_srcdir)/ChangeLog Makefile
	@echo Making $@
	@echo "#ifndef GNUPLOT_TIMEBASE_H_INCLUDED" >$@t
	@echo "#define GNUPLOT_TIMEBASE_H_INCLUDED" >>$@t
	@head -1 $< | sed -e 's,\(^[0-9-]* \).*$$,const char gnuplot_date[] = "\1";,' >>$@t
	@echo "#endif /* GNUPLOT_TIMEBASE_H_INCLUDED */" >> $@t
	@if cmp -s $@ $@t; then rm -f $@t; else mv $@t $@; fi

if BUILD_QT

GNUPLOT_QT = `echo gnuplot_qt | sed '$(transform)'`$(EXEEXT)

AM_CPPFLAGS += -DGNUPLOT_QT=\"$(GNUPLOT_QT)\"

# Call the moc preprocessor
qtterminal/moc_%.cpp : qtterminal/%.h
	$(MOC) -o $@ $<

# Call the Qt resource compiler
qtterminal/qrc_%.cpp: qtterminal/%.qrc
	$(RCC) -name QtGnuplotResource -o $@ $<

# Call the Qt ui compiler
ui_%.h: qtterminal/%.ui
	$(UIC) -o $@ $<

if HAVE_LRELEASE

# Generate translation files
%.qm: qtterminal/po/%.ts
	$(LRELEASE) $< -qm $@

# List of compiles translation files
TRANSLATIONS = qtgnuplot_fr.qm qtgnuplot_ja.qm

# Install translation files
qtgnuplotdir = $(pkgdatadir)/$(VERSION_MAJOR)/qt
qtgnuplot_DATA = $(TRANSLATIONS)
AM_CPPFLAGS += -DQTGNUPLOT_DATA_DIR=\"$(qtgnuplotdir)\"

endif # HAVE_LRELEASE

# Clean moc & qrc files
clean-qt-extra:
	rm -vf qtterminal/moc_*.cpp qtterminal/qrc_*.cpp qtterminal/qrc_*.o ui_*.h qtgnuplot*.qm


# Clean transform gnuplot_qt for demo
clean-demo:
	rm -f $(GNUPLOT_QT)

clean-local: clean-qt-extra clean-demo

# Qt terminal sources

gnuplot_SOURCES += qtterminal/qt_term.cpp

pkglibexec_PROGRAMS += gnuplot_qt

$(gnuplot_qt_OBJECTS) : ui_QtGnuplotSettings.h

nodist_gnuplot_qt_SOURCES = \
        qtterminal/moc_QtGnuplotWindow.cpp qtterminal/moc_QtGnuplotApplication.cpp \
        qtterminal/moc_QtGnuplotWidget.cpp qtterminal/moc_QtGnuplotScene.cpp \
        qtterminal/moc_QtGnuplotEvent.cpp qtterminal/qrc_QtGnuplotResource.cpp

gnuplot_qt_SOURCES = qtterminal/gnuplot_qt.cpp qtterminal/QtGnuplotWindow.cpp \
qtterminal/QtGnuplotApplication.cpp qtterminal/QtGnuplotWidget.cpp \
qtterminal/QtGnuplotScene.cpp qtterminal/QtGnuplotItems.cpp \
qtterminal/QtGnuplotEvent.cpp
gnuplot_qt_LDADD = $(QT_LIBS)

# embed example (uncomment to build it)

# bin_PROGRAMS += embed_example
# 
# nodist_embed_example_SOURCES = moc_embed_example.cpp moc_QtGnuplotInstance.cpp \
#                          moc_QtGnuplotWidget.cpp moc_QtGnuplotScene.cpp \
#                          moc_QtGnuplotEvent.cpp qrc_QtGnuplotResource.cpp
# 
# embed_example_SOURCES = qtterminal/embed_example.cpp \
# qtterminal/QtGnuplotWidget.cpp qtterminal/QtGnuplotEvent.cpp \
# qtterminal/QtGnuplotScene.cpp qtterminal/QtGnuplotItems.cpp \
# qtterminal/QtGnuplotInstance.cpp
# 
# embed_example_LDADD = $(QT_LIBS)

endif # build_qt

## we don't need COREOBJS, but CORETERM
## for the term.o dependencies
O=o
T=$(top_srcdir)/term/
# begin makefile.all
include makefile.all
# end makefile.all
term$U.o: $(CORETERM)

# Provide an explicit build recipe for bf_test so that a zillion extraneous
# libraries are not dragged in.
bf_test$(EXEEXT): $(bf_test_OBJECTS)
	@rm -f bf_test$(EXEEXT)
	$(AM_V_CCLD)$(LINK) $(bf_test_OBJECTS) $(bf_test_LIBS)

