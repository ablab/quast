# Makefile.am for GNUPLOT documentation directory
# based on Makefile.in as of 20-may-2012
#
# Note that the master file for generating documentation is gnuplot.doc.
# See README.
#
# To print manual:
#
#   For LaTeX
#       make dvi
#     or
#       make pdf
#	(requires pdflatex and also graphicx and picins packages)
#     or
#       make pdf_nofig
#       (PDF without figures, e.g. if graphicx or picins are not present)
#     or
#       make ps
#
#   For groff (GNU troff}
#       make grotxt
#       (print or view gnuplot.txt)
#     or
#       make grodvi
#       (print or view gnuplot.dvi)
#     or
#       make grops
#       (print or view gnuplot.ps)
#     or
#       make groff
#       (all three of the above)
#
#   For nroff
#     make nroff
#     (print or view gnuplot.nroff)
#
#   For troff
#     make "TROFF=itroff" troff    (use your troff here)
#
#   For GNU info (texinfo)
#     make info
#

# default is what is needed for interactive gnuplot
gih_DATA = gnuplot.gih
gihdir = @GIHDIR@

noinst_PROGRAMS = checkdoc doc2ipf doc2tex doc2gih doc2rnh doc2hlp \
                  doc2rtf doc2ms alldoc2gih doc2wxhtml

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/term

T = $(top_srcdir)/term/
CORETERM = $(T)ai.trm $(T)aquaterm.trm $(T)be.trm \
$(T)cairo.trm $(T)canvas.trm $(T)cgm.trm $(T)context.trm \
$(T)corel.trm $(T)debug.trm $(T)djsvga.trm $(T)dumb.trm $(T)dxf.trm \
$(T)dxy.trm $(T)eepic.trm $(T)emf.trm $(T)emxvga.trm $(T)epson.trm \
$(T)estimate.trm $(T)excl.trm $(T)fig.trm $(T)gd.trm $(T)ggi.trm \
$(T)gpic.trm $(T)grass.trm $(T)hp2648.trm $(T)hp26.trm $(T)hp500c.trm \
$(T)hpgl.trm $(T)hpljii.trm $(T)hppj.trm $(T)imagen.trm $(T)kyo.trm \
$(T)latex.trm $(T)linux.trm $(T)lua.trm $(T)metafont.trm \
$(T)metapost.trm $(T)mif.trm $(T)next.trm $(T)openstep.trm $(T)pbm.trm \
$(T)pdf.trm $(T)pm.trm $(T)post.trm $(T)pslatex.trm \
$(T)pstricks.trm $(T)qms.trm $(T)qt.trm $(T)regis.trm $(T)sun.trm \
$(T)svg.trm $(T)t410x.trm $(T)tek.trm $(T)texdraw.trm $(T)tgif.trm \
$(T)tkcanvas.trm $(T)tpic.trm $(T)vgagl.trm \
$(T)vws.trm $(T)win.trm $(T)wxt.trm $(T)x11.trm $(T)xlib.trm 

# DIST_COMMON = README

SOURCES = checkdoc.c doc2gih.c doc2hlp.c doc2ipf.c doc2ms.c doc2rnh.c	\
doc2rtf.c doc2tex.c windows/doc2html.c termdoc.c xref.c

TERM_H = $(top_srcdir)/src/term.h

DOCHEADERS = doc2x.h xref.h

EXTRA_DIST = Makefile.in doc2texi.el figure_missing.pdf gnuplot.doc \
gpcard.tex makefile.dst titlepag.ipf titlepag.ms titlepag.tex	\
toc_entr.sty psdoc faq-ja.tex gnuplot-ja.doc README.ja	\
term-ja.diff plotstyles.gnu gnuplot.pdf windows

ELCS = gnuplot-eldoc.elc

MOSTLYCLEANFILES = gnuplot.aux gnuplot.ind gnuplot.log gnuplot.out	\
gnuplot.toc figures.aux figures.log figures.toc figures.idx		\
figures.ilg figures.ind figures.out nofigures.aux nofigures.log		\
nofigures.toc nofigures.idx nofigures.ilg nofigures.ind nofigures.out	\
gpcard.log pdffigures.tex pdffigures.aux wxhelp/doc2html.o

CLEANFILES = allterm.h gnuplot.dvi figures.tex nofigures.tex		\
gnuplot.tex gnuplot.idx gnuplot.ilg gnuplot.ps gpcard.dvi gpcard.ps	\
gnuplot.gih gnuplot.hlp gnuplot.info* gnuplot.ipf gnuplot.ms		\
gnuplot.rnh gnuplot.rtf gnuplot.nroff gnuplot.txt gnuplot-groff.dvi	\
gnuplot-groff.ps gnuplot.tmp VERSION gnuplot.4tc gnuplot.4ct		\
gnuplot.idv gnuplot.xref gnuplot.lg gnuplot.pdf wxhelp_figures		\
wxhelp/*.html wxhelp/wgnuplot.hhc wxhelp/wgnuplot.hhk			\
wxhelp/wgnuplot.htc wxhelp/doc2html$(EXEEXT) windows/wgnuplot.htk	\
windows/*.png gnuplot.htb gnuplot.texi gnuplot-eldoc.el $(ELCS)

DISTFILES = $(DIST_COMMON) $(SOURCES) $(DOCHEADERS) $(EXTRA_DIST)

BUILT_SOURCES = allterm.h

GNUPLOT_EXE = ../src/gnuplot$(EXEEXT)
SRC_VERSION_O   = ../src/version.o

$(GNUPLOT_EXE):
	( cd ../src ; $(MAKE) gnuplot$(EXEEXT) )
$(SRC_VERSION_O):
	( cd ../src ; $(MAKE) version.o )

if BUILD_LUA
LUA_HELP = gnuplot-tikz.help
$(LUA_HELP): $(top_srcdir)/term/lua/gnuplot-tikz.lua
	$(AM_V_GEN)lua $< termhelp > $@
else
LUA_HELP = 
endif

allterm.h: $(CORETERM) $(LUA_HELP)
	$(AM_V_GEN) for e in `egrep "^[ 	]*START_HELP" $(CORETERM) |\
	     LC_ALL=C sort -f -t':' -k2` ; do \
	  f=`echo $$e |cut -d\: -f1` ; s=`echo $$e | cut -d\: -f2` ;\
	  sed -n "/^[ 	]*$$s/,/^[ 	]*END_HELP/p" $$f ; \
	done >$@

# for debugging
alldoc: check dvi gih hlp html info ipf nroff pdf ps rnh rtf htb

### [tn]roff documentation
troff: gnuplot.ms $(srcdir)/titlepag.ms
	$(AM_V_GEN) tbl gnuplot.ms | eqn | $(TROFF) -ms

nroff: gnuplot.nroff
gnuplot.nroff: gnuplot.ms $(srcdir)/titlepag.ms
	$(AM_V_GEN) tbl $< | neqn | nroff -ms | col > $@

### groff documentation

groff: grotxt grodvi grops

grotxt: gnuplot.txt
gnuplot.txt: gnuplot.ms $(srcdir)/titlepag.ms
	$(AM_V_GEN) groff -s -p -t -e -ms -Tlatin1 $< > $@

grodvi: gnuplot-groff.dvi
gnuplot-groff.dvi: gnuplot.ms $(srcdir)/titlepag.ms
	$(AM_V_GEN) groff -s -p -t -e -ms -Tdvi $< > $@

grops: gnuplot-groff.ps
gnuplot-groff.ps: gnuplot.ms $(srcdir)/titlepag.ms
	$(AM_V_GEN) groff -s -p -t -e -ms -Tps $< > $@ 

### doc2xxx dependencies
gnuplot.ms: doc2ms$(EXEEXT) $(srcdir)/gnuplot.doc
	$(AM_V_GEN) ./doc2ms$(EXEEXT) $(srcdir)/gnuplot.doc gnuplot.ms "$(srcdir)/titlepag.ms"

doc2ms_SOURCES = doc2ms.c termdoc.c
doc2ms_CPPFLAGS = -DALL_TERM_DOC $(AM_CPPFLAGS)

### html
html: htmldocs/gnuplot.html
htmldocs/gnuplot.html: figures.tex wxhelp_figures pdf_figures
	$(AM_V_GEN)if test $(LATEX2HTML) != no ; then \
	  mkdir -p htmldocs ; \
	  $(LATEX2HTML) -dir htmldocs -local_icons $< ; \
	else \
	  echo latex2html not found - cannot build html file ; \
	fi

### PDF documentation
pdf_figures: $(GNUPLOT_EXE) $(srcdir)/plotstyles.gnu
	$(AM_V_at)GNUPLOT_LIB=$(top_srcdir)/demo $(GNUPLOT_EXE) $(srcdir)/plotstyles.gnu
	$(AM_V_GEN)touch $@

figures.tex: allterm.h doc2tex$(EXEEXT) $(srcdir)/gnuplot.doc
	$(AM_V_GEN)./doc2tex$(EXEEXT) -figures $(srcdir)/gnuplot.doc $@

nofigures.tex: allterm.h doc2tex$(EXEEXT) $(srcdir)/gnuplot.doc
	$(AM_V_GEN)./doc2tex$(EXEEXT) $(srcdir)/gnuplot.doc $@

pdf: gnuplot.pdf
pdf_nofig: nofigures.pdf

gnuplot.pdf: figures.tex pdf_figures $(srcdir)/titlepag.tex $(srcdir)/toc_entr.sty
	$(AM_V_GEN)if test $(PDFLATEX) != no ; then \
	  rm -f figures.idx ; \
	  touch figures.idx ; \
	  rm -f pdffigures.tex ; \
	  ( echo "\usepackage{graphicx}" > pdffigures.tex ; \
	    echo "\usepackage{picins}" >> pdffigures.tex ; \
	  ) ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(PDFLATEX) $< ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(PDFLATEX) $< ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(PDFLATEX) $< ; \
	  makeindex figures ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(PDFLATEX) $< ; \
	  mv figures.pdf $@ ; \
	  rm -f pdffigurex.tex pdffigures.aux ; \
	else \
	  echo pdflatex not found - cannot build pdf file ; \
	fi

nofigures.pdf: nofigures.tex $(srcdir)/titlepag.tex $(srcdir)/toc_entr.sty
	$(AM_V_GEN)if test $(PDFLATEX) != no ; then \
	  rm -f nofigures.idx ; \
	  touch nofigures.idx ; \
	  rm -f pdffigures.tex ; \
	  echo "" > pdffigures.tex ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(PDFLATEX) $< ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(PDFLATEX) $< ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(PDFLATEX) $< ; \
	  makeindex nofigures ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(PDFLATEX) $< ; \
	  rm -f pdffigurex.tex pdffigures.aux ; \
	else \
	  echo pdflatex not found - cannot build pdf file ; \
	fi

### LaTeX documentation
tex: gnuplot.tex
gnuplot.tex: nofigures.tex
	$(AM_V_GEN) $(LN_S) $< $@

doc2tex_SOURCES = doc2tex.c termdoc.c
doc2tex_CPPFLAGS = -DALL_TERM_DOC $(AM_CPPFLAGS)

dvi: gnuplot.dvi
gnuplot.dvi: gnuplot.tex $(srcdir)/titlepag.tex $(srcdir)/toc_entr.sty
	$(AM_V_GEN)if test $(LATEX) != no ; then \
	  rm -f nofigures.idx ; \
	  touch nofigures.idx ; \
	  rm -f pdffigures.tex ; \
	  echo "" > pdffigures.tex ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(LATEX) $< ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(LATEX) $< ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(LATEX) $< ; \
	  makeindex gnuplot ; \
	  TEXINPUTS=.:$(srcdir):$(top_srcdir):${TEXINPUTS}: $(LATEX) $< ; \
	  rm -f pdffigurex.tex pdffigures.aux ; \
	else \
	  echo "latex not found - cannot build dvi file" ; \
	fi

gpcard.dvi: $(srcdir)/gpcard.tex
	$(AM_V_GEN)if test $(PLAINTEX) != no ; then \
              TEXINPUTS=.:$(srcdir):$(top_srcdir): tex gpcard ; \
	else \
	  echo "plaintex not found - cannot build dvi file" ; \
	fi

### Postscript format
# if pslatex has been installed, add "times" to titlepage.tex
ps: gnuplot.ps gpcard.ps

gnuplot.ps: gnuplot.dvi
	$(AM_V_GEN)if test $(DVIPS) != no ; then \
	   $(DVIPS) gnuplot.dvi -o gnuplot.ps ; \
	else \
	   echo "dvips not found - cannot build PS file" ; \
	fi

gpcard.ps: gpcard.dvi
	$(AM_V_GEN)if test $(DVIPS) != no ; then \
            $(DVIPS) gpcard.dvi -o gpcard.ps ; \
	else \
	   echo "dvips not found - cannot build PS file" ; \
	fi


# this is how to make gnuplot.hlp
hlp: gnuplot.hlp

gnuplot.hlp: doc2hlp$(EXEEXT) $(srcdir)/gnuplot.doc
	$(AM_V_GEN)./doc2hlp$(EXEEXT) $(srcdir)/gnuplot.doc gnuplot.hlp

doc2hlp_SOURCES = doc2hlp.c termdoc.c

### gnuplot interactive help format
gih: gnuplot.gih

gnuplot.gih: doc2gih$(EXEEXT) $(srcdir)/gnuplot.doc
	$(AM_V_GEN)./doc2gih$(EXEEXT) $(srcdir)/gnuplot.doc gnuplot.gih

doc2gih_SOURCES = doc2gih.c termdoc.c

# To include all terminals in the .gih file
allgih: alldoc2gih$(EXEEXT) $(srcdir)/gnuplot.doc
	@echo "generate gnuplot.gih with all terminals"
	$(AM_V_at)./alldoc2gih$(EXEEXT) $(srcdir)/gnuplot.doc gnuplot.gih

alldoc2gih_SOURCES = doc2gih.c termdoc.c
alldoc2gih_CPPFLAGS = -DALL_TERM_DOC $(AM_CPPFLAGS)

### wxhelp format
htb: gnuplot.htb

gnuplot.htb: wxhelp/wgnuplot.html wxhelp_figures	\
$(srcdir)/windows/wgnuplot.hhp wxhelp/wgnuplot.hhc	\
$(srcdir)/windows/wgnuplot.hhk
	$(AM_V_GEN) zip -j9 gnuplot.htb windows/wgnuplot.hhp		\
wxhelp/wgnuplot.hhc wxhelp/wgnuplot.hhk wxhelp/*.html windows/*.png

wxhelp/wgnuplot.html: doc2wxhtml$(EXEEXT) $(srcdir)/gnuplot.doc 
	$(AM_V_GEN) ./doc2wxhtml$(EXEEXT) $(srcdir)/gnuplot.doc wxhelp/

doc2wxhtml$(EXEEXT): wxhelp/doc2html.o termdoc.o xref.o $(SRC_VERSION_O)
	$(AM_V_CCLD) $(LINK) $^

wxhelp/doc2html.o: $(srcdir)/windows/doc2html.c $(BUILT_SOURCES)
	$(AM_V_AT)[ -d wxhelp ] || mkdir wxhelp
	$(AM_V_CC) $(COMPILE) -c -o $@ -I.. -DALL_TERM_DOC -DWXHELP $<


# HBB FIXME 2015-06-28: this is how the above really should be handled, but it causes
# problems with the way automake handles its 'subdir-objects' option.
#doc2wxhtml_SOURCES = windows/doc2html.c termdoc.c xref.c allterm.h ../src/version.c
#doc2wxhtml_CPPFLAGS = -DALL_TERM_DOC -DWXHELP -I../src $(AM_CPPFLAGS)

wxhelp_figures: $(GNUPLOT_EXE) $(srcdir)/plotstyles.gnu
	$(AM_V_at) [ -d windows ] || mkdir -p windows
	$(AM_V_at) GNUPLOT_LIB=$(top_srcdir)/demo $(GNUPLOT_EXE) -e "winhelp=1;" $(srcdir)/plotstyles.gnu
	$(AM_V_GEN)touch $@

### GNU info format
info: gnuplot.info

# Thanks to Bruce Ravel for doc2texi.el!
gnuplot.texi gnuplot-eldoc.el: $(srcdir)/doc2texi.el $(srcdir)/gnuplot.doc
	$(AM_V_GEN) if test "$(EMACS)" != no; then \
	   test "$(top_srcdir)" = "$(top_builddir)" || cp $(srcdir)/gnuplot.doc . ; \
	   $(EMACS) -batch -q --no-site-file -l $< -f d2t-doc-to-texi ; \
	   test "$(top_srcdir)" = "$(top_builddir)" || rm -f ./gnuplot.doc ; \
	else \
	   echo "No emacs found - cannot create texinfo file" ; \
	fi

${ELCS}: gnuplot-eldoc.el
	$(AM_V_GEN) if test "$(EMACS)" != no; then \
	   echo "Compiling gnuplot-eldoc.el" ; \
	   $(EMACS) -batch -q --no-site-file -f batch-byte-compile $* ; \
	else \
	   echo "No emacs found - cannot create gnuplot-eldoc.elc file" ; \
	fi

gnuplot.info: gnuplot.texi
	$(AM_V_GEN) $(MAKEINFO) -I$(srcdir) $< --no-split --output=$@

install-info: gnuplot.info
	$(AM_V_at) $(top_srcdir)/mkinstalldirs $(DESTDIR)$(infodir)
	$(AM_V_at) $(INSTALL_DATA) gnuplot.info $(DESTDIR)$(infodir)/gnuplot.info
	$(AM_V_at) if (install-info --version && \
	     install-info --version | fgrep -i -v debian) >/dev/null 2>&1; then \
	  install-info --info-dir=$(DESTDIR)$(infodir) $(DESTDIR)$(infodir)/gnuplot.info || : ;\
	else : ; fi

# this is how to make OS/2 ipfc documentation
ipf: gnuplot.ipf

gnuplot.ipf: doc2ipf$(EXEEXT) $(srcdir)/gnuplot.doc
	$(AM_V_GEN) ./doc2ipf$(EXEEXT) $(srcdir)/gnuplot.doc gnuplot.ipf

doc2ipf_SOURCES = doc2ipf.c termdoc.c xref.c

### Rich Text Format
rtf: gnuplot.rtf

gnuplot.rtf: doc2rtf$(EXEEXT) $(srcdir)/gnuplot.doc
	$(AM_V_GEN) ./doc2rtf$(EXEEXT) $(srcdir)/gnuplot.doc gnuplot.rtf

doc2rtf_SOURCES = doc2rtf.c termdoc.c xref.c

# this is how to make rnh documentation (VMS help format)
rnh: gnuplot.rnh

gnuplot.rnh: doc2rnh$(EXEEXT) $(srcdir)/gnuplot.doc
	$(AM_V_GEN) ./doc2rnh$(EXEEXT) $(srcdir)/gnuplot.doc gnuplot.rnh

doc2rnh_SOURCES = doc2rnh.c termdoc.c

# this is how to check the gnuplot.doc file
check-local: checkdoc$(EXEEXT)
	$(AM_V_at)./checkdoc$(EXEEXT) < $(srcdir)/gnuplot.doc; \
	if test $$? -eq 0; then \
	  echo "PASS: gnuplot.doc"; \
	else \
	  :; \
	fi

checkdoc_SOURCES = checkdoc.c termdoc.c gnuplot.doc

clean-local:
	$(AM_V_at)for f in figure*.pdf $(LUA_HELP); do \
	   if [ "$$f" != "figure_missing.pdf" ]; then \
	     rm -f $$f ; \
	   else \
	     :; \
	   fi \
	 done
	$(AM_V_at)rm -f pdf_figures

# for VMS only
install-vms: gnuplot.hlp
	$(AM_V_at)$(INSTALL_DATA) gnuplot.hlp $(datadir)/gnuplot.hlp

#the end
