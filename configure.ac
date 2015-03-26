AC_PREREQ([2.65])
AC_INIT(jackpunkconsole, 0.1)
AC_CONFIG_SRCDIR([src/main.c])

AM_INIT_AUTOMAKE

AC_CONFIG_HEADERS([config.h])

AC_PROG_CC

AC_CONFIG_MACRO_DIR(m4)

AC_CHECK_LIB([jack], [jack_client_open], [], [
        echo "Error: libjack is missing."
        exit -1
        ])
AC_CHECK_LIB(pthread, pthread_create, [], [
        echo "Error: pthread is missing."
        exit -1
])

AM_PATH_GTK_3_0(3.0.0,[AC_DEFINE(HAVE_GTK,1,Define to 1 if you have the gtk library.)])

AC_OUTPUT(Makefile src/Makefile)
