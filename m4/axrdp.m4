# SYNOPSIS
#
#   AXRDP_CHECK_UTMPX_MEMBER_EXISTS(MEMBER, COMPILE-DEFINE)
#
# EXAMPLE
#
# AXRDP_CHECK_UTMPX_MEMBER_EXISTS([ut_exit], [HAVE_UTMPX_UT_EXIT])
#
# DESCRIPTION
#
#   If the member MEMBER exists in the utmpx struct, the COMPILE-DEFINE
#   is set for the C compiler.
#
#   The shell variable 'ac_cv_utmpx_has_$MEMBER' is set to 'yes' or 'no'
#   and cached
#
AC_DEFUN([AXRDP_CHECK_UTMPX_MEMBER_EXISTS],
[
  AS_VAR_PUSHDEF([x_var], [ac_cv_utmpx_has_$1])
  AS_VAR_PUSHDEF([x_define], [$2])
  AC_CACHE_CHECK(
      [for $1 in struct utmpx],
      [x_var],
      [AC_COMPILE_IFELSE(
         [AC_LANG_SOURCE([[
#           include <utmpx.h>
#           include <stddef.h>
            int main()
            {
                return offsetof(struct utmpx,$1);
            }]])],
         [AS_VAR_SET([x_var], [yes])],
         [AS_VAR_SET([x_var], [no])])]
  )
  AS_VAR_IF(
    [x_var],
    [yes],
    [AC_DEFINE([x_define], [1], [Define if '$1' is in struct utmpx.])])
  AS_VAR_POPDEF([x_var])
  AS_VAR_POPDEF([x_define])
])


