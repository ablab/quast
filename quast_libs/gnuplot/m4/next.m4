## ------------------------------- ##
## Check for NeXT.                 ##
## From Lars Hecking               ##
## ------------------------------- ##

# serial 1

AC_DEFUN([GP_NEXT],
[AC_MSG_CHECKING(for NeXT)
AC_EGREP_CPP(yes,
[#if __NeXT__
  yes
#endif
], AC_MSG_RESULT(yes)
   LIBS="$LIBS -lsys_s -lNeXT_s"
   CFLAGS="$CFLAGS -ObjC"
   is_next=yes,
   AC_MSG_RESULT(no)
   is_next=no)
])

