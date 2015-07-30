AC_PREREQ([2.69])
AC_INIT([filemanager-httpd], [0.1], [fatwildcat@gmail.com])
AC_CONFIG_SRCDIR([src/fmconfig.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([limits.h netinet/in.h stdlib.h string.h strings.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRCOLL
AC_CHECK_FUNCS([gethostname memchr mkdir rmdir select socket strcasecmp strchr strcspn strdup strerror strncasecmp strrchr strspn strstr strtoul])

AC_CONFIG_FILES([Makefile src/Makefile debian/Makefile])
AC_OUTPUT
