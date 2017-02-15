## Process this file with automake to produce Makefile.in -*-Makefile-*-
AUTOMAKE_OPTIONS = foreign

##m4-files-begin
EXTRA_DIST = README Makefile.am.in alpha.m4 apple.m4 beos.m4 msdos.m4 \
next.m4 pkg.m4 
##m4-files-end

Makefile.am: Makefile.am.in
	rm -f $@ $@t
	sed -n '1,/^##m4-files-begin/p' Makefile.am.in > $@t
	echo EXTRA_DIST = README Makefile.am.in *.m4 | fmt | \
	  (tr '\012' @; echo ) \
	  |sed 's/@$$/%/;s/@/ \\@/g' |tr @% '\012 ' \
	  >> $@t
	sed -n '/^##m4-files-end/,$$p' Makefile.am.in >> $@t
	chmod og-w $@t
	mv $@t $@

