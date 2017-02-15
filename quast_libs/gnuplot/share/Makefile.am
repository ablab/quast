## Process this file with automake to produce Makefile.in -*-Makefile-*-
AUTOMAKE_OPTIONS = foreign
                                                                                
EXTRA_DIST = Gnuplot.app-defaults gnuplotrc \
             colors_podo.gp colors_default.gp colors_mono.gp

if HAVE_LATEX
SUBDIRS = LaTeX
endif

appdefaultdir = $(pkgdatadir)/$(VERSION_MAJOR)/app-defaults

APPDEFAULTFILES = Gnuplot

sharedir = $(pkgdatadir)/$(VERSION_MAJOR)

share_DATA = colors_default.gp colors_podo.gp colors_mono.gp gnuplotrc

if BUILD_GNUPLOT_X11
appdefault_DATA = $(APPDEFAULTFILES)
else
appdefault_DATA =
endif

Gnuplot: $(srcdir)/Gnuplot.app-defaults
	cp -p $(srcdir)/Gnuplot.app-defaults $@

CLEANFILES = $(APPDEFAULTFILES)
