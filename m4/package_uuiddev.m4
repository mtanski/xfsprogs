AC_DEFUN([AC_PACKAGE_NEED_UUID_UUID_H],
  [ AC_CHECK_HEADERS([uuid/uuid.h])
    if test "$ac_cv_header_uuid_uuid_h" != yes; then
	echo
	echo 'FATAL ERROR: could not find a valid UUID header.'
	echo 'Install the Universally Unique Identifiers development package.'
	exit 1
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_UUIDPARSE_LIBUUID],
  [ AC_CHECK_LIB(uuid, uuid_parse,, [
	echo
	echo 'FATAL ERROR: could not find a valid UUID library.'
	echo 'Install the Universally Unique Identifiers library package.'
	exit 1
    ])
    libuuid="/usr/lib/libuuid.a"
    AC_SUBST(libuuid)
  ])
