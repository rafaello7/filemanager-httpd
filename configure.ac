AC_PREREQ([2.69])
AC_INIT([filemanager-httpd], [0.3.5], [fatwildcat@gmail.com])
AC_CONFIG_SRCDIR([src/fmconfig.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netinet/in.h stdint.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRCOLL
AC_CHECK_FUNCS([dup2 gethostname memchr memset mkdir rmdir select socket strcasecmp strchr strcspn strdup strerror strncasecmp strrchr strspn strstr strtoul strtoull])

AC_CONFIG_FILES([Makefile src/Makefile conf.d/Makefile])
AC_OUTPUT
