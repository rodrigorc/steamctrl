AC_INIT([steamctrl], [1.0], [rodrigorivascosta@gmail.com])
AC_CONFIG_AUX_DIR([auxdir])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AM_SILENT_RULES([yes])

AC_SUBST(AM_CPPFLAGS, "")
AC_SUBST(AM_CFLAGS, "-Wall")

AC_PROG_CC

PKG_CHECK_MODULES(UDEV, libudev)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
