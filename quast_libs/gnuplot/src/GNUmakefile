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

