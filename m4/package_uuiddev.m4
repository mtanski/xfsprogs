AC_DEFUN([AC_PACKAGE_NEED_UUID_H],
  [ AC_CHECK_HEADERS(uuid.h)
    if test $ac_cv_header_uuid_h = no; then
	AC_CHECK_HEADERS(uuid/uuid.h,, [
	echo
	echo 'FATAL ERROR: could not find a valid UUID header.'
	echo 'Install the Universally Unique Identifiers development package.'
	exit 1])
    fi
  ])

AC_DEFUN([AC_PACKAGE_CHECK_LIBUUID],
  [ AC_CHECK_FUNCS(uuid_compare)
    if test $ac_cv_func_uuid_compare = yes; then
	libuuid=""
    elif test "$enable_shared_uuid" = no; then
	AC_MSG_CHECKING([for libuuid])
	OLDLIBS="$LIBS"
	UUIDLIBS="/usr/lib/libuuid.a /usr/lib64/libuuid.a"
	for uuidlib in $UUIDLIBS; do
	    LIBS="$OLDLIBS $uuidlib"
	    AC_LINK_IFELSE([AC_LANG_PROGRAM(, [ uuid_compare(); ])],
			   [ libuuid="$uuidlib" ], [ continue ],)
	    AC_MSG_RESULT($libuuid)
	done
	if test -z "$libuuid"; then
	    AC_MSG_RESULT(not found)
	    echo
	    echo 'FATAL ERROR: could not find a valid UUID library.'
	    echo 'Install the Universally Unique Identifiers library package.'
	    exit 1
	fi
	LIBS="$OLDLIBS"
    else
	libuuid="-luuid"
    fi
  ])
