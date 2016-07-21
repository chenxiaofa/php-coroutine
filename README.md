# php-coroutine
php的coroutine扩展 支持多层函数调用中yield



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
##Methods
###Coroutine::__construct($callable)
###Coroutine::resume()
###Coroutine::yield()
###Coroutine::running()
###Coroutine::reset();

##Properties
###$status
