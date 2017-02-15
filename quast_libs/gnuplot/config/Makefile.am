## Process this file with automake to produce Makefile.in -*-Makefile-*-
AUTOMAKE_OPTIONS = foreign

# empty
all:

##m4-files-begin
EXTRA_DIST = README MacOSX Makefile.am.in buildvms.com config.cyg \
config.dj2 config.mgw config.nt config.os2 config.oww config.wc \
djconfig.sh make_vms.com makefile.cyg makefile.dj2 makefile.emx makefile.g \
makefile.os2 makefile.vms makefile.wc watcom/Makefile watcom/config.h \
cygwin/Makefile mingw/Makefile msvc/Makefile 
##m4-files-end

Makefile.am: Makefile.am.in
	rm -f $@ $@t
	sed -n '1,/^##m4-files-begin/p' Makefile.am.in > $@t
	echo EXTRA_DIST = README MacOSX Makefile.am.in buildvms.com	\
	  config.* djconfig.sh make_vms.com makefile.* watcom/Makefile	\
	  watcom/config.h cygwin/Makefile mingw/Makefile msvc/Makefile \
	  | fmt \
          | (tr '\012' @; echo) \
	  | sed 's/@$$/%/;s/@/ \\@/g' \
          | tr @% '\012 ' >> $@t
	sed -n '/^##m4-files-end/,$$p' $< >> $@t
	chmod og-w $@t
	mv $@t $@

