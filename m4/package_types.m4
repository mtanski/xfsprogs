# 
# Check if we have a type for the pointer's size integer (__psint_t)
# 
AC_DEFUN([AC_TYPE_PSINT],
  [ AC_MSG_CHECKING([for __psint_t ])
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
    ], [
         __psint_t  psint;
    ], AC_DEFINE(HAVE___PSINT_T) AC_MSG_RESULT(yes) , AC_MSG_RESULT(no))
  ])

# 
# Check if we have a type for the pointer's size unsigned (__psunsigned_t)
# 
AC_DEFUN([AC_TYPE_PSUNSIGNED],
  [ AC_MSG_CHECKING([for __psunsigned_t ])
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
    ], [
        __psunsigned_t  psuint;
    ], AC_DEFINE(HAVE___PSUNSIGNED_T) AC_MSG_RESULT(yes) , AC_MSG_RESULT(no))
  ])

# 
# Check if we have a type for __u32
# 
AC_DEFUN([AC_TYPE_U32],
  [ AC_MSG_CHECKING([for __u32 ])
    AC_TRY_COMPILE([
#include <asm/types.h>
#include <stdlib.h>
#include <stddef.h>
    ], [
         __u32  u32;
    ], AC_DEFINE(HAVE___U32) AC_MSG_RESULT(yes) , AC_MSG_RESULT(no))
  ])
