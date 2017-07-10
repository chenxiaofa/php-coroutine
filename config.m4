
PHP_ARG_WITH(coroutine, for coroutine support,
Make sure that the comment is aligned:
[  --with-coroutine             Include coroutine support])


if test "$PHP_COROUTINE" != "no"; then

  PHP_NEW_EXTENSION(coroutine, php_coroutine.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
