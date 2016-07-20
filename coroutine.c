/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/
 
/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "SAPI.h"

#include "php_ini.h"
#include "ext/standard/info.h"

#include "ext/standard/basic_functions.h"

#include "php_coroutine.h"


enum COROUTINE_STATUS{
	COROUTINE_STATUS_SUSPEND,
	COROUTINE_STATUS_RUNNING,
	COROUTINE_STATUS_DEAD,
	COROUTINE_IN_MAIN,
	COROUTINE_IN_COROUTINE
};


/* 备份当前执行环境 */
/* {{{ coroutine_backup_executor 
 */
void zend_always_inline coroutine_backup_executor()
{
	/* Backup executor globals */
	CO_G(main_execute_data) = EG(current_execute_data);
	CO_G(main_scope) 		= EG(scope);
	CO_G(main_stack) 		= EG(vm_stack);
	CO_G(main_stack)->top 	= EG(vm_stack_top);
	CO_G(shutdown_function_names) = BG(user_shutdown_function_names);
}
/* }}} */


/* 恢复执行环境 */
/* {{{ coroutine_restore_executor 
 */
void zend_always_inline coroutine_restore_executor()
{
	/* Restore executor globals */
	EG(current_execute_data) 	= CO_G(main_execute_data);
	EG(scope) 					= CO_G(main_scope);
	EG(vm_stack_top) 			= CO_G(main_stack)->top;
	EG(vm_stack_end) 			= CO_G(main_stack)->end;
	EG(vm_stack) 				= CO_G(main_stack);
	BG(user_shutdown_function_names)  =	CO_G(shutdown_function_names);
}
/* }}} */

/* 协程挂起处理函数 */
/* {{{ COROUTINE_SUSPEND_HANDLER 
 */
void ZEND_FASTCALL COROUTINE_SUSPEND_HANDLER(ZEND_OPCODE_HANDLER_ARGS)
{
	zend_op *next_op = (zend_op*)OPLINE;
	next_op->handler = CO_G(cache_handler);
	SAVE_OPLINE();
	CO_G(cache_handler) = NULL;
	OPLINE = NULL;
	CURRCO(status) = COROUTINE_STATUS_SUSPEND;
	CURRCO(execute_data) = EG(current_execute_data);
	return;
}
/* }}} */


//typedef void (ZEND_FASTCALL *opcode_handler_t) (ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);


/* 释放协程中函数调用栈 */
/* {{{ coroutine_free_vm_stack_call_frame 
 */
void zend_always_inline coroutine_free_vm_stack_call_frame(zend_execute_data *ex)
{
		
	while(ex)
	{ 
		zend_vm_stack_free_call_frame(ex);
		ex = ex->prev_execute_data;
	}
}
/* }}} */

/* 重置协程函数调用栈 */
/* {{{ coroutine_vm_stack_reset 
 */
void zend_always_inline coroutine_vm_stack_reset(zend_vm_stack stack)
{
	while (stack->prev != NULL) { 
		zend_vm_stack p = stack->prev; 
		efree(stack);
		stack = p;
	}
}
/* }}} */




/* 创建协程上下文环境 */
/* {{{ coroutine_build_execute_data 
 */
void zend_always_inline coroutine_build_execute_data(coroutine_context *ctx)
{
	coroutine_backup_executor();
	

	ctx->status = COROUTINE_STATUS_SUSPEND;


	EG(vm_stack)	 = ctx->stack;
	EG(vm_stack_top) = ctx->stack->top;
	EG(vm_stack_end) = ctx->stack->end; 
 
	zend_execute_data *ex = ctx->execute_data?ctx->execute_data:ctx->top_execute_data;
	
	coroutine_free_vm_stack_call_frame(ex);
	 
	coroutine_vm_stack_reset(ctx->stack);

 	uint32_t call_info = ZEND_CALL_TOP_FUNCTION | ctx->fci_cache->function_handler->common.fn_flags ; 
	if (ctx->fci_cache->object) {
		call_info |= ZEND_CALL_RELEASE_THIS; 
	}

	 
	ctx->top_execute_data = ctx->execute_data = zend_vm_stack_push_call_frame(call_info, 
		(zend_function*)&ctx->fci_cache->function_handler->op_array, 
		0,
		ctx->fci_cache->called_scope,
		ctx->fci_cache->object
	);


	
	
	ctx->top_execute_data->symbol_table = NULL;

	i_init_execute_data(ctx->execute_data, &ctx->fci_cache->function_handler->op_array, NULL);
	ctx->execute_data->prev_execute_data = NULL;

	ctx->stack      = EG(vm_stack); 
	ctx->stack->top = EG(vm_stack_top);
	ctx->stack->end = EG(vm_stack_end);
	

	coroutine_restore_executor();
	
}
/* }}} */



/* 销毁协程上下文环境 */
/* {{{ coroutine_destory_context 
 */
void zend_always_inline coroutine_destory_context(coroutine_context *ctx)
{
	//php_printf("coroutine_destory_context(ctx=%X)",ctx);

	coroutine_backup_executor();
	
	/* Set executor globals */
	EG(vm_stack_top) = ctx->stack->top;
	EG(vm_stack_end) = ctx->stack->end;
	EG(vm_stack) = ctx->stack;


	if (ZEND_CALL_INFO(ctx->top_execute_data) & ZEND_CALL_RELEASE_THIS)
	{
		
		zend_object *object = Z_OBJ(ctx->top_execute_data->This);
		OBJ_RELEASE(object);
	}

	zend_execute_data *ex = ctx->execute_data?ctx->execute_data:ctx->top_execute_data;
	coroutine_free_vm_stack_call_frame(ex);


	zend_vm_stack_destroy();
	efree(ctx->fci_cache);
	efree(ctx);
	
	coroutine_restore_executor();
	
	
	
}
/* }}} */


/* 申请协程栈 */
/* {{{ coroutine_vm_stack_new_page 
 */
static zend_always_inline zend_vm_stack coroutine_vm_stack_new_page() {
	zend_vm_stack page = (zend_vm_stack)emalloc(COROUTINE_VM_STACK_INIT_SIZE);
	page->top = ZEND_VM_STACK_ELEMETS(page);
	page->end = (zval*)((char*)page + COROUTINE_VM_STACK_INIT_SIZE);
	page->prev = NULL;
	return page;
}
/* }}} */


/* 初始化协程上下文 */
/* {{{ coroutine_init_context 
 */
static zend_always_inline void coroutine_init_context(zend_object *object,coroutine_context *ctx,zend_fcall_info_cache *fci_cache)
{
	ctx->shutdown_function_names = NULL;
	ctx->stack = coroutine_vm_stack_new_page();
	ctx->status = COROUTINE_STATUS_SUSPEND; 

	ctx->top_execute_data = ctx->execute_data = NULL;

	ctx->this_obj = object;
	
	ctx->fci_cache = fci_cache;

	coroutine_build_execute_data(ctx);
}
/* }}} */


/* 唤醒协程 */
/* {{{ resume_coroutine 
 */
void resume_coroutine(coroutine_context *ctx)
{
	CO_G(context) = ctx;

	/* Backup executor globals */
	coroutine_backup_executor();


	ctx->top_execute_data->prev_execute_data = EG(current_execute_data);
	ctx->status = COROUTINE_STATUS_RUNNING;

	
	/* Set executor globals */
	EG(current_execute_data) = ctx->execute_data;
	EG(scope) = ctx->execute_data->func->common.scope;
	EG(vm_stack_top) = ctx->stack->top;
	EG(vm_stack_end) = ctx->stack->end;
	EG(vm_stack) = ctx->stack;
	BG(user_shutdown_function_names)  = ctx->shutdown_function_names;


	
	



	/*** execute ***/
	zend_execute_ex(ctx->execute_data);
	



	if (ctx->status == COROUTINE_STATUS_RUNNING)
	{		
		ctx->execute_data = NULL;
		ctx->status = COROUTINE_STATUS_DEAD;
		php_call_shutdown_functions();
		php_free_shutdown_functions();
	}

	
	ctx->top_execute_data->prev_execute_data = NULL;

	
	ctx->stack = EG(vm_stack);
	ctx->stack->top = EG(vm_stack_top);
	ctx->shutdown_function_names = BG(user_shutdown_function_names);


	coroutine_restore_executor();
	
	CO_G(context) = NULL;
}
/* }}} */



/* CoPHP class entry */
zend_class_entry *coroutine_ce;


/**  CoPHP Method */

/* {{{ CoThread::__construct()
 */
ZEND_METHOD(coroutine,__construct)
{

	zval *callback = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &callback) == FAILURE) { 
		return;
	}
	
	zend_fcall_info_cache *fci_cache;
	fci_cache = (zend_fcall_info_cache *)emalloc(sizeof(zend_fcall_info_cache));
	if (!fci_cache)
	{
		zend_error(E_ERROR,"alloc memory failed.");
		RETURN_FALSE;
	}
	zend_string *callable_name;
	char *error = NULL;
		
	if (!zend_is_callable_ex(callback,NULL,IS_CALLABLE_CHECK_SILENT,&callable_name,fci_cache,&error)){
		if (error) {
			zend_error(E_ERROR, "Invalid callback %s, %s", ZSTR_VAL(callable_name), error);
			efree(error);
		}
		if (callable_name) {
			zend_string_release(callable_name);
		}
		efree(fci_cache);
		RETURN_FALSE;
	} else if (error) {
		/* Capitalize the first latter of the error message */
		if (error[0] >= 'a' && error[0] <= 'z') {
			error[0] += ('A' - 'a');
		}
		zend_error(E_DEPRECATED, "%s", error);
		efree(error);
	}
	
	zend_string_release(callable_name);

	//创建coroutine执行上下文
	coroutine_context *context = (coroutine_context *)emalloc(sizeof(coroutine_context));
	zend_update_property_long(coroutine_ce ,getThis(),"context",7,(zend_long)context); 
	zend_update_property(coroutine_ce ,getThis(),"callable",8,callback); 
	coroutine_init_context(Z_OBJ_P(getThis()),context,fci_cache);
	

//	php_printf("created coroutine context:%ld\n",context);
//	php_printf("OBJ=%ld\n",Z_OBJ_P(getThis()));


}
/* }}} */




/* {{{ CoThread::__construct()
 */
ZEND_METHOD(coroutine,__destruct) 
{
	coroutine_destory_context(Z_COROUTINE_CONTEXT_P(getThis()));
}
/* }}} */




/* {{{ CoThread::yield() 
 */
ZEND_METHOD(coroutine,yield) 
{ 
	//php_printf("current_execute_data:%ld\n",EG(current_execute_data));


	if (CO_G(context)==NULL || CO_G(context)->status == COROUTINE_STATUS_SUSPEND)
	{
		RETURN_FALSE;
	}
	
	zend_op *next_op = (zend_op*)(OPLINE+1);
	CO_G(cache_handler) = (void*)next_op->handler;
	next_op->handler = COROUTINE_SUSPEND_HANDLER;
}
/* }}} */


/* {{{ CoThread::reset() 
 */
ZEND_METHOD(coroutine,reset)
{
	if (CO_G(context) != NULL )
	{
		RETURN_NULL();
	}
	
	coroutine_context * ctx = Z_COROUTINE_CONTEXT_P(getThis());
	
	if (ctx->status != COROUTINE_STATUS_DEAD)
	{
		RETURN_NULL();
	}

	coroutine_build_execute_data(ctx);
	zend_update_property_long(coroutine_ce,getThis(),"status",6,ctx->status);
}
/* }}} */


/* {{{ CoThread::running() 
 */
ZEND_METHOD(coroutine,running)
{
	if (CO_G(context) == NULL)
	{
		RETURN_NULL();
	}
	ZVAL_OBJ(return_value, CO_G(context)->this_obj);
	Z_TRY_ADDREF_P(return_value);
	return;
}
/* }}} */

/* {{{ CoThread::running() 
 */
ZEND_METHOD(coroutine,resume)
{

	
	if (CO_G(context) != NULL)
	{
		RETURN_FALSE;
	}

	
	
	coroutine_context * ctx = Z_COROUTINE_CONTEXT_P(getThis());

	
	if (ctx->status == COROUTINE_STATUS_DEAD)
	{
		RETURN_FALSE;
	}
	resume_coroutine(ctx);
	zend_update_property_long(coroutine_ce,getThis(),"status",6,ctx->status);
}
/* }}} */



/* CoThread class method entry */
/* {{{ coroutine_method */
static zend_function_entry coroutine_method[] = {
	ZEND_ME(coroutine,  	yield						,  NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(coroutine,  	running		,  NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(coroutine,	__construct					,  NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	ZEND_ME(coroutine,	__destruct					,  NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_DTOR|ZEND_ACC_FINAL)
	ZEND_ME(coroutine,	resume						,  NULL,   ZEND_ACC_PUBLIC)
	ZEND_ME(coroutine,	reset						,  NULL,   ZEND_ACC_PUBLIC)

    { NULL, NULL, NULL }
};
/* }}} */


/* If you declare any globals in php_coroutine.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(coroutine)
*/

/* True global resources - no need for thread safety here */
static int le_coroutine;

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("coroutine.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_coroutine_globals, coroutine_globals)
    STD_PHP_INI_ENTRY("coroutine.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_coroutine_globals, coroutine_globals)
PHP_INI_END()
*/
/* }}} */



/* {{{ php_coroutine_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_coroutine_init_globals(zend_coroutine_globals *coroutine_globals)
{
	coroutine_globals->global_value = 0;
	coroutine_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(coroutine)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
	
	zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Coroutine",coroutine_method);
    coroutine_ce = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_class_constant_long(coroutine_ce, "STATUS_RUNNING", 	strlen("STATUS_RUNNING"),	COROUTINE_STATUS_RUNNING TSRMLS_CC);
	zend_declare_class_constant_long(coroutine_ce, "STATUS_SUSPEND", 	strlen("STATUS_SUSPEND"),	COROUTINE_STATUS_SUSPEND TSRMLS_CC);
	zend_declare_class_constant_long(coroutine_ce, "STATUS_DEAD", 		strlen("STATUS_DEAD"),		COROUTINE_STATUS_DEAD TSRMLS_CC);
	
    zend_declare_property_long(coroutine_ce, "status", strlen("status"),COROUTINE_STATUS_SUSPEND, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(coroutine_ce, "context", strlen("context"),0, ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_long(coroutine_ce, "callable", strlen("callable"),0, ZEND_ACC_PRIVATE TSRMLS_CC);

	
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(coroutine)
{
	/* uncomment this line if you have INI entriescache_handler
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */




/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(coroutine)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "coroutine support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */



/* {{{ coroutine_functions[]
 *
 * Every user visible function must have an entry in coroutine_functions[].
 */
const zend_function_entry coroutine_functions[] = {
	PHP_FE_END	/* Must be the last line in coroutine_functions[] */
};
/* }}} */

/* {{{ coroutine_module_entry
 */
zend_module_entry coroutine_module_entry = {
	STANDARD_MODULE_HEADER,
	"coroutine",
	coroutine_functions,
	PHP_MINIT(coroutine),
	PHP_MSHUTDOWN(coroutine),
	NULL,//PHP_RINIT(coroutine),		/* Replace with NULL if there's nothing to do at request start */
	NULL,//PHP_RSHUTDOWN(coroutine),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(coroutine),
	PHP_COROUTINE_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_COROUTINE
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif

ZEND_GET_MODULE(coroutine)
#endif




/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
