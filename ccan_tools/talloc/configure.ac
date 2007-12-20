AC_PREREQ(2.50)
AC_INIT(talloc, 1.1.0)
AC_CONFIG_SRCDIR([talloc.c])
AC_SUBST(datarootdir)
AC_CONFIG_HEADER(config.h)

m4_include(libtalloc.m4)

AC_PATH_PROG(XSLTPROC,xsltproc)
DOC_TARGET=""
if test -n "$XSLTPROC"; then
	DOC_TARGET=doc
fi
AC_SUBST(DOC_TARGET)

AC_OUTPUT(Makefile talloc.pc)
