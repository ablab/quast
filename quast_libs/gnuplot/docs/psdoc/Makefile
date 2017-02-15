#
# Create ps_symbols.ps
#

top_srcdir = ../..

all: ps_symbols.ps ps_fontfile_doc.ps

pdf: ps_fontfile_doc.pdf

ps_symbols.ps: ps_symbols.gpi
	@if test -f $(top_srcdir)/src/gnuplot ; then \
		$(top_srcdir)/src/gnuplot ps_symbols.gpi ; \
	else \
		gnuplot ps_symbols.gpi ; \
	fi

ps_fontfile_doc.ps: ps_fontfile_doc.tex
	latex ps_fontfile_doc
	latex ps_fontfile_doc
	dvips -j0 ps_fontfile_doc

ps_fontfile_doc.pdf: ps_fontfile_doc.tex
	pdflatex ps_fontfile_doc
	pdflatex ps_fontfile_doc

clean:
	-rm -f ps_symbols.ps ps_fontfile_doc.aux ps_fontfile_doc.dvi \
	       ps_fontfile_doc.log ps_fontfile_doc.pdf ps_fontfile_doc.ps

distclean: clean

