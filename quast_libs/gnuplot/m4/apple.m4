# GP_APPLE
# -------------------------------
# Sets is_apple={yes, no}
# Checks for Aquaterm framework if is_apple is 'yes',
# adds framework to LDFLAGS and -ObjC to CFLAGS
AC_DEFUN([GP_APPLE],[
  AC_MSG_CHECKING([for Mac OS X])
  AC_EGREP_CPP(yes,[
#if defined(__APPLE__) && defined(__MACH__)
      yes
#endif
  ],[
    AC_MSG_RESULT([yes])
    is_apple=yes
    AC_ARG_WITH(aquaterm,
                [  --with-aquaterm         include support for AquaTerm on OSX],
                [if test "$withval" == yes; then
                    GP_HAVE_FRAMEWORK(AquaTerm,[#import <AquaTerm/AQTAdapter.h>],[],
                                      [CFLAGS="$CFLAGS -ObjC"; LDFLAGS="$LDFLAGS -framework Foundation -framework AquaTerm"],[])
                fi])
  ],[
    AC_MSG_RESULT([no])
    is_apple=no
  ])dnl AC_EGREP_CPP
])

# GP_HAVE_FRAMEWORK(name, [prologue], [body], [action-if-found], [action-if-not-found])
# -------------------------------
# Check for framework 'name' by trying to compile a program composed
# of prologue and body while linking to the framework
# DEFINE HAVE_FRAMEWORK_NAME=1 if test passes.
# This macro should be generally useful.
AC_DEFUN([GP_HAVE_FRAMEWORK],[
  AC_MSG_CHECKING([for $1 framework presence])
  ac_gnuplot_save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -framework Foundation -framework $1"
  AC_LANG_PUSH(Objective C)
  AC_LINK_IFELSE([AC_LANG_PROGRAM([$2], [$3])],
                 eval "gnuplot_framework_$1=yes",
                 eval "gnuplot_framework_$1=no")
  AC_LANG_POP(Objective C)
  LDFLAGS="$ac_gnuplot_save_LDFLAGS"
  if test "$gnuplot_framework_$1" = yes; then
    AC_DEFINE(m4_toupper(HAVE_FRAMEWORK_$1),1,[Define to 1 if you're using the $1 framework on Mac OS X])
    AC_MSG_RESULT([yes])
    [$4]
  else
    AC_MSG_RESULT([no])
    [$5]
  fi
])
