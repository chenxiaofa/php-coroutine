dnl $Id$
dnl config.m4 for extension coroutine

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

 PHP_ARG_WITH(coroutine, for coroutine support,
 Make sure that the comment is aligned:
 [  --with-coroutine             Include coroutine support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(coroutine, whether to enable coroutine support,
dnl Make sure that the comment is aligned:
dnl [  --enable-coroutine           Enable coroutine support])

if test "$PHP_COROUTINE" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-coroutine -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/coroutine.h"  # you most likely want to change this
  dnl if test -r $PHP_COROUTINE/$SEARCH_FOR; then # path given as parameter
  dnl   COROUTINE_DIR=$PHP_COROUTINE
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for coroutine files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       COROUTINE_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$COROUTINE_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the coroutine distribution])
  dnl fi

  dnl # --with-coroutine -> add include path
  dnl PHP_ADD_INCLUDE($COROUTINE_DIR/include)

  dnl # --with-coroutine -> check for lib and symbol presence
  dnl LIBNAME=coroutine # you may want to change this
  dnl LIBSYMBOL=coroutine # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $COROUTINE_DIR/$PHP_LIBDIR, COROUTINE_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_COROUTINELIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong coroutine lib version or lib not found])
  dnl ],[
  dnl   -L$COROUTINE_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(COROUTINE_SHARED_LIBADD)

  PHP_NEW_EXTENSION(coroutine, coroutine.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
