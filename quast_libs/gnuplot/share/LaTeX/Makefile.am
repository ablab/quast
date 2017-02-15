## Process this file with automake to produce Makefile.in -*-Makefile-*-
AUTOMAKE_OPTIONS = foreign

EXTRA_DIST = README ${tex_DATA}


if BUILD_LUA
     build_lua = BUILD_LUA
     lua_files = gnuplot-lua-tikz.sty gnuplot-lua-tikz-common.tex gnuplot-lua-tikz.tex t-gnuplot-lua-tikz.tex
else
     build_lua =
     lua_files =
endif

tex_DATA = gnuplot.cfg ${lua_files}
texdir = @TEXDIR@

${lua_files}:
	test -z "$(build_lua)" || lua $(top_srcdir)/term/lua/gnuplot-tikz.lua style

install-data-hook:
	test -n "$(DESTDIR)" || $(TEXHASH)

uninstall-hook: 
	test -n "$(DESTDIR)" || $(TEXHASH)

