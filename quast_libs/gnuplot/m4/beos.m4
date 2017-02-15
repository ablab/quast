## ------------------------------- ##
## Check for BeOS.                 ##
## From Lars Hecking               ##
## From Xavier Pianet              ##
## ------------------------------- ##

# serial 1

AC_DEFUN([GP_BEOS],
[AC_MSG_CHECKING(for BeOS)
AC_EGREP_CPP(yes,
[#if __BEOS__
  yes
#endif
], AC_MSG_RESULT(yes)
   build_src_beos_subdir=yes,
   build_src_beos_subdir=no
   AC_MSG_RESULT(no))
])

