dnl $Id$
dnl config.m4 for extension cophp

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

 PHP_ARG_WITH(cophp, for cophp support,
 Make sure that the comment is aligned:
 [  --with-cophp             Include cophp support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(cophp, whether to enable cophp support,
dnl Make sure that the comment is aligned:
dnl [  --enable-cophp           Enable cophp support])

if test "$PHP_COPHP" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-cophp -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/cophp.h"  # you most likely want to change this
  dnl if test -r $PHP_COPHP/$SEARCH_FOR; then # path given as parameter
  dnl   COPHP_DIR=$PHP_COPHP
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for cophp files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       COPHP_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$COPHP_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the cophp distribution])
  dnl fi

  dnl # --with-cophp -> add include path
  dnl PHP_ADD_INCLUDE($COPHP_DIR/include)

  dnl # --with-cophp -> check for lib and symbol presence
  dnl LIBNAME=cophp # you may want to change this
  dnl LIBSYMBOL=cophp # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $COPHP_DIR/$PHP_LIBDIR, COPHP_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_COPHPLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong cophp lib version or lib not found])
  dnl ],[
  dnl   -L$COPHP_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(COPHP_SHARED_LIBADD)

  PHP_NEW_EXTENSION(cophp, cophp.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
