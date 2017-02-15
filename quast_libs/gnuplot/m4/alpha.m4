## ------------------------------- ##
## Check for Alpha AXP.            ##
## By Hans-Bernhard Broeker        ##
## (edited copy of msdos.m4)       ##
## ------------------------------- ##

# serial 1

AC_DEFUN([GP_ALPHA],
[AC_MSG_CHECKING(for Alpha/AXP CPU)
AC_EGREP_CPP(yes,
[#ifdef __alpha
  yes
#endif
],AC_MSG_RESULT(yes)
  if test "$GCC" = "yes" ; then
     CFLAGS="-mieee $CFLAGS"
  else
     CFLAGS="-ieee $CFLAGS"
  fi,
  AC_MSG_RESULT(no)
  )dnl
])

