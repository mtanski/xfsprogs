AC_DEFUN([AC_PACKAGE_NEED_UUID_UUID_H],
  [ AC_CHECK_HEADERS(uuid/uuid.h,, [
	AC_CHECK_HEADER(uuid.h,, [
	echo
	echo 'FATAL ERROR: could not find a valid UUID header.'
	echo 'Install the Universally Unique Identifiers development package.'
	exit 1])
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_UUIDCOMPARE_LIBUUID],
  [ AC_CHECK_LIB(uuid, uuid_compare,, [
	AC_CHECK_FUNCS(uuid_compare,, [
	echo
	echo 'FATAL ERROR: could not find a valid UUID library.'
	echo 'Install the Universally Unique Identifiers library package.'
	exit 1])
    ])
    libuuid="/usr/lib/libuuid.a"
    AC_SUBST(libuuid)
  ])
