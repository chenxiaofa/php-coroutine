# php-coroutine
php的coroutine扩展
##使用方法
```php

function a()
{
  echo 1;
  Coroutine::yield();
  echo 3;
}

$co = new Coroutine(function(){
  a();
});
$co->resume();
echo 2;
$co->resume();

result:123
```
