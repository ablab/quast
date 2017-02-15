$ ! for batch operation, set default to the gnuplot directory
$ !
$ ! buildvms.com
$ ! Command file to compile/link gnuplot, gnuplot_x11, and make gnuplot.hlb
$ !
$ ! lph: modified for compatibility with VMS 4.x (which lacks 'if ... endif'),
$ ! but made the default DECC
$ !
$! set noon
$ ON ERROR THEN GOTO FINISH
$!
$! detect compiler - drd
$! if DECC is around, assume that, else gcc is preferred. Finally vaxc
$ its_decc = (f$search("SYS$SYSTEM:DECC$COMPILER.EXE") .nes. "")
$ its_gnuc = 0	 ! comment out the next line to use VAXC if gcc is also present 
$ its_gnuc = .not.its_decc .and. (f$trnlnm("gnu_cc").nes."")
$ its_vaxc = .not. (its_decc .or. its_gnuc)
$ its_decw = (f$trnlnm("DECW$INCLUDE") .nes. "") 
$!
$! configure
$
$ pfix = "/prefix=all"
$ rtl  = "DECCRTL"
$   if .NOT. its_decc then pfix = "/nolist"
$   if .NOT. its_decc then rtl  = "VAXCRTL"
$!
$ x11 = ""
$ if its_decw then x11 = "X11,"
$! 
$!-----------------------------------------------------------------
$!-----------------------------------------------------------------
$! customize CFLAGS for version of VMS, CRTL, and C compiler.
$!-----------------------------------------------------------------
$ IF F$GETSYI("ARCH_TYPE") .EQ. 1
$ THEN
$!  these defines work for OpenVMS VAX v6.2 and DEC C v5.7
$ CFLAGS = "/define=(PROTOTYPES,HAVE_UNISTD_H,HAVE_GETCWD,"-
 +"HAVE_SLEEP,''x11'NO_GIH,PIPES,DECCRTL)''pfix'"
$ ELSE
$!  these defines work for OpenVMS Alpha v6.2 and DEC C v5.3
$ CFLAGS = "/define=(PROTOTYPES,HAVE_LGAMMA,HAVE_ERF,HAVE_UNISTD_H,HAVE_GETCWD,"-
 +"HAVE_SLEEP,''x11'NO_GIH,PIPES,DECCRTL)''pfix'"
$ ENDIF
$!
$!-----------------------------------------------------------------
$!
$! A generic starting point 
$!-----------------------------------------------------------------
$!
$!$ CFLAGS = "/NOWARN/NOOP/DEFINE=(''x11'NO_GIH,PIPES,''rtl')''pfix'"
$!
$! ----------------------------------------------------------------
$!
$! For  VMS 4.7 and VAX C v2.4  
$! ("Compiler abort - virtual memory limits exceeded" if attempt
$!  to include all applicable terminals, but otherwise builds OK.  
$!  Runtime problem: an exit handler error, also w/ gcc build;
$!  a VAXCRTL atexit bug?)
$!
$! Note: VAX uses  D_FLOAT, maximum exponent ca 10e +/- 38;
$!       will cause problems with some of the demos
$!
$!$ CFLAGS    = "/NOOP/DEFINE=(HAVE_GETCWD, HAVE_SLEEP, "-
$!               +"SHORT_TERMLIST, NO_GIH,PIPES, ''rtl')"
$!$!
$!
$!-----------------------------------------------------------------
$!
$! This will build with gcc v1.42 on VMS 4.7
$! (no virtual memory limit problem)
$!
$! gcc v1.42 string.h can prefix str routines w/ gnu_ (ifdef GCC_STRINGS)
$! but the routines in GCCLIB are not prefixed w/ gcc_  :-(
$! link with GCCLIB, then ignore the link warnings about multiple
$! definitions of STR... in C$STRINGS
$!
$! GCC v1.42 has a locale.h, but neither gcc nor VMS v4.7 VAXCRTL has 
$! the  setlocale function
$!           
$!
$! Note: _assert.c defines assert_gcc, if ndef NDEBUG, but
$!        cgm.trm undefines NDEBUG, so we always compile/link  _assert.c
$!
$!$ CFLAGS    = "/NOOP/DEFINE=(''x11', HAVE_GETCWD,"-
$!		+" HAVE_SLEEP, NO_GIH, PIPES, ''rtl')"
$!
$!-----------------------------------------------------------------
$!-----------------------------------------------------------------
$!
$ TERMFLAGS = "/INCLUDE=([],[.-term])"
$
$ EXTRALIB = ""
$ if its_gnuc then cc := GCC/NOCASE
$ if its_gnuc then EXTRALIB = ",[]_assert,GNU_CC:[000000]GCCLIB/LIB"
$ 
$
$ CFLAGS="''cflags'" + "''pfix'"
$ LINKOPT=""
$!
$ if .NOT. its_decc then -
      LINKOPT=",sys$disk:[]linkopt.vms/opt"
$!
$!
$ if its_decw then DEFINE/NOLOG X11 DECW$INCLUDE
$ if its_decw then DEFINE/NOLOG SYS SYS$LIBRARY
$!
$ set verify
$ cc 'CFLAGS' alloc.c
$ cc 'CFLAGS' binary.c
$ cc 'CFLAGS' bitmap.c
$ cc 'CFLAGS' command.c
$ cc 'CFLAGS' contour.c
$ cc 'CFLAGS' datafile.c
$ cc 'CFLAGS' dynarray.c
$ cc 'CFLAGS' eval.c
$ cc 'CFLAGS' fit.c
$ cc 'CFLAGS' graph3d.c
$ cc 'CFLAGS' graphics.c
$ cc 'CFLAGS' help.c
$ cc 'CFLAGS' hidden3d.c
$ cc 'CFLAGS' history.c
$ cc 'CFLAGS' internal.c
$ cc 'CFLAGS' interpol.c
$ cc 'CFLAGS' matrix.c
$ cc 'cflags' misc.c
$ cc 'CFLAGS' parse.c
$ cc 'CFLAGS' plot.c
$ cc 'CFLAGS' plot2d.c
$ cc 'CFLAGS' plot3d.c
$ cc 'CFLAGS' save.c
$ cc 'CFLAGS' scanner.c
$ cc 'CFLAGS' set.c
$ cc 'CFLAGS' show.c
$ cc 'CFLAGS' specfun.c
$ cc 'CFLAGS' standard.c
$ cc 'CFLAGS' stdfn.c
$ cc 'CFLAGS' tables.c
$ cc 'cflags' 'TERMFLAGS' term.c
$ cc 'cflags' time.c
$ cc 'CFLAGS' unset.c
$ cc 'CFLAGS' util.c
$ cc 'CFLAGS' util3d.c
$ cc 'CFLAGS' variable.c
$ cc 'CFLAGS' vms.c
$ cc 'CFLAGS' version.c
$ if its_gnuc then cc 'CFLAGS' GNU_CC_INCLUDE:[000000]_assert.c
$!
$ link/exe=gnuplot.exe -
alloc.obj,binary.obj,bitmap.obj,command.obj,contour.obj,dynarray.obj,-
datafile.obj,eval.obj,fit.obj,graphics.obj,graph3d.obj,help.obj,hidden3d.obj,-
history.obj,internal.obj,interpol.obj,matrix.obj,misc.obj,parse.obj,plot.obj,-
plot2d.obj,plot3d.obj,save.obj,scanner.obj,set.obj,show.obj,specfun.obj,-
standard.obj,stdfn.obj,tables.obj,tabulate.obj,term.obj,time.obj,util.obj,util3d.obj,-
unset.obj,variable.obj,version.obj,vms.obj'extralib''LINKOPT'
$!
$ cc 'CFLAGS' bf_test.c
$ link /exe=bf_test bf_test,binary,alloc 'extralib''LINKOPT'
$ cd [-.demo]
$ run [-.src]bf_test.exe
$ cd [-.src]
$ if .NOT. its_decw  then goto do_docs 
$!
$ CC 'CFLAGS' GPLT_X11
$ CC 'CFLAGS' stdfn.c
$ LINK /exe=GNUPLOT_X11 gplt_x11 'extralib''LINKOPT',SYS$INPUT:/OPT
SYS$SHARE:DECW$XLIBSHR/SHARE
$!
$DO_DOCS:
$ SET DEF [-.DOCS]
$ if f$locate("ALL_TERM_DOC",CFLAGS).ne.f$length(CFLAGS) then -
	copy /concatenate [-.term]*.trm []allterm.h
$ cc 'CFLAGS' /OBJ=doc2rnh.obj/include=([],[-],[-.term],[-.src]) doc2rnh.c 
$ cc 'CFLAGS' /OBJ=termdoc.obj/include=([],[-],[-.term],[-.src]) termdoc.c 
$ SET DEF [-.src]		! LINKOPT is defined as being in []
$ link [-.docs]doc2rnh.obj,termdoc.obj /exe=[-.docs]doc2rnh 'extralib''LINKOPT'
$ doc2rnh := $sys$disk:[-.docs]doc2rnh
$ doc2rnh [-.docs]gnuplot.doc [-.docs]gnuplot.rnh
$ RUNOFF [-.docs]gnuplot.rnh
$ library/create/help sys$disk:[]gnuplot.hlb gnuplot.hlp
$!
$ if its_decw then -
  write sys$output "%define GNUPLOT_X11 :== $Disk:[directory]GNUPLOT_X11"
$!
$FINISH:
$ set noverify
$ if its_decw then deassign x11
$ exit
