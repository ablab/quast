## Process this file with automake to produce Makefile.in -*-Makefile-*-
AUTOMAKE_OPTIONS = foreign

SUBDIRS = config m4 term src docs $(LISPDIR) man demo tutorial share

EXTRA_DIST = BUGS CodeStyle Copyright FAQ.pdf GNUmakefile INSTALL INSTALL.gnu \
Makefile.maint PATCHLEVEL PGPKEYS PORTING README RELEASE_NOTES \
VERSION configure.vms pm3d win

DISTCLEANFILES = stamp-h

ACLOCAL_AMFLAGS = -I m4

dist-hook:
	@for dir in `find $(distdir) -type d -name CVS -print`; do \
	  rm -rf $$dir; done
	@find $(distdir) -type d -name .deps -print | xargs rm -rf
	@find $(distdir) -type f -name .cvsignore -print | xargs rm -f

gnuplot:
	cd src; make binonly

