# Having a separate GNUmakefile lets me `include' the dynamically
# generated rules created via Makefile.maint as well as Makefile.maint itself.
# This makefile is used only if you run GNU Make.
# It is necessary if you want to build targets usually of interest
# only to the maintainer.

# Systems where /bin/sh is not the default shell need this.  The $(shell)
# command below won't work with e.g. stock DOS/Windows shells.
SHELL = /bin/sh

have-Makefile := $(shell test -f Makefile && echo yes)

# If the user runs GNU make but has not yet run ./configure,
# give them a diagnostic.
ifeq ($(have-Makefile),yes)

include Makefile
include $(srcdir)/Makefile.maint

else

all:
	@echo There seems to be no Makefile in this directory.
	@echo "You must run ./configure before running \`make'."
	@exit 1

endif

pre-dist:
	@cd docs && make gnuplot.texi
	@cd config && rm -f Makefile.am && make -f Makefile.am.in Makefile.am
	@cd demo && rm -f Makefile.am && make -f Makefile.am.in Makefile.am
	@cd m4 && rm -f Makefile.am && make -f Makefile.am.in Makefile.am
	@cd src && rm -f makefile.all && make -f Makefile.maint makefile.all
	@cd term && rm -f Makefile.am && make -f Makefile.am.in Makefile.am
	@cd tutorial && rm -f Makefile.am && make -f Makefile.am.in Makefile.am
