## ------------------------------- ##
## Check for MS-DOS/djgpp.         ##
## From Lars Hecking and           ##
## Hans-Bernhard Broeker           ##
## ------------------------------- ##

# serial 1

AC_DEFUN([GP_MSDOS],
[AC_MSG_CHECKING(for MS-DOS/djgpp/libGRX)
AC_EGREP_CPP(yes,
[#if __DJGPP__ && __DJGPP__ == 2
  yes
#endif
],AC_MSG_RESULT(yes)
  LIBS="-lpc $LIBS"
  AC_DEFINE(MSDOS, 1,
            [ Define if this is an MSDOS system. ])
  AC_DEFINE(DOS32, 1,
            [ Define if this system uses a 32-bit DOS extender (djgpp/emx). ])
  with_linux_vga=no
  AC_CHECK_LIB(grx20,GrLine,
    LIBS="-lgrx20 $LIBS"
    CFLAGS="$CFLAGS -fno-inline-functions"
    AC_DEFINE(DJSVGA, 1,
              [ Define if you want to use libgrx20 with MSDOS/djgpp. ])
    AC_CHECK_LIB(grx20,GrCustomLine,
      AC_DEFINE(GRX21, 1,
                [ Define if you want to use a newer version of libgrx under MSDOS/djgpp. ])dnl
    )dnl
  )
  is_msdos=yes,
  AC_MSG_RESULT(no)
  is_msdos=no
  )dnl
])

