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

开始\恢复 Coroutine运行;

###Coroutine::yield()

中断Coroutine,程序返回到调用 resume的点

###Coroutine::running()
###Coroutine::reset();

##Properties
###$status

当前协程状态
Access:Public

###Constants
STATUS_SUSPEND = 0;
STATUS_RUNNING = 1;
STATUS_DEAD = 2;

#注意
1. 当前版本不支持在协程中对另一个协程resume操作
2. 协程中会对register_shutdown_function的函数进行拦截,并在协程结束后调用
