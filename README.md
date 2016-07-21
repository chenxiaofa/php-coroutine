# php-coroutine (Beta)
php的coroutine(协程\用户态线程)扩展 支持多层函数调用中yield



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

1. 传入函数名:$co = new Coroutine("run");
2. 传入类方法:$co = new Coroutine([$object,"methodName"]);
3. 传入闭包(Closure):$co = new Coroutine(function(){});

###Coroutine::resume()
###Coroutine::yield()
###Coroutine::running()
###Coroutine::reset();

##Properties
###$status
