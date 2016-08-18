<?php
/**
 * 珠海魅族科技有限公司
 * Copyright (c) 2012 - 2013 meizu.com ZhuHai Inc. (http://www.meizu.com)
 * User: 陈晓发
 * Date: 2016/7/11
 * Time: 17:28
 */



$a = new Coroutine(function(){
	setcookie("asda",'aadasda',1,2,3,4,5,6,7,8,9,10);
});
$a->hook('setcookie',function(){
	print_r(func_get_args());
});
$a->resume();