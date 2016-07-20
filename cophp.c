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

#include "php_cophp.h"


enum COTHREAD_STATUS{
	COTHREAD_STATUS_SUSPEND,
	COTHREAD_STATUS_RUNNING,
	COTHREAD_STATUS_DEAD,
	COTHREAD_IN_MAIN,
	COTHREAD_IN_COTHREAD
};


/* 备份当前执行环境 */
/* {{{ cothread_backup_executor 
 */
void zend_always_inline cothread_backup_executor()
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
/* {{{ cothread_restore_executor 
 */
void zend_always_inline cothread_restore_executor()
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
/* {{{ COTHREAD_SUSPEND_HANDLER 
 */
void ZEND_FASTCALL COTHREAD_SUSPEND_HANDLER(ZEND_OPCODE_HANDLER_ARGS)
{
	zend_op *next_op = (zend_op*)OPLINE;
	next_op->handler = CO_G(cache_handler);
	SAVE_OPLINE();
	CO_G(cache_handler) = NULL;
	OPLINE = NULL;
	CURRCO(status) = COTHREAD_STATUS_SUSPEND;
	CURRCO(execute_data) = EG(current_execute_data);
	return;
}
/* }}} */


//typedef void (ZEND_FASTCALL *opcode_handler_t) (ZEND_OPCODE_HANDLER_ARGS_PASSTHRU);


/* 释放协程中函数调用栈 */
/* {{{ cothread_free_vm_stack_call_frame 
 */
void zend_always_inline cothread_free_vm_stack_call_frame(zend_execute_data *ex)
{
		
	while(ex)
	{ 
		zend_vm_stack_free_call_frame(ex);
		ex = ex->prev_execute_data;
	}
}
/* }}} */

/* 重置协程函数调用栈 */
/* {{{ cothread_vm_stack_reset 
 */
void zend_always_inline cothread_vm_stack_reset(zend_vm_stack stack)
{
	while (stack->prev != NULL) { 
		zend_vm_stack p = stack->prev; 
		efree(stack);
		stack = p;
	}
}
/* }}} */




/* 创建协程上下文环境 */
/* {{{ cothread_build_execute_data 
 */
void zend_always_inline cothread_build_execute_data(cothread_context *ctx)
{
	cothread_backup_executor();
	

	ctx->status = COTHREAD_STATUS_SUSPEND;


	EG(vm_stack)	 = ctx->stack;
	EG(vm_stack_top) = ctx->stack->top;
	EG(vm_stack_end) = ctx->stack->end; 
 
	zend_execute_data *ex = ctx->execute_data?ctx->execute_data:ctx->top_execute_data;
	
	cothread_free_vm_stack_call_frame(ex);
	 
	cothread_vm_stack_reset(ctx->stack);

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
	

	cothread_restore_executor();
	
}
/* }}} */



/* 销毁协程上下文环境 */
/* {{{ cothread_destory_context 
 */
void zend_always_inline cothread_destory_context(cothread_context *ctx)
{
	//php_printf("cothread_destory_context(ctx=%X)",ctx);

	cothread_backup_executor();
	
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
	cothread_free_vm_stack_call_frame(ex);


	zend_vm_stack_destroy();
	efree(ctx->fci_cache);
	efree(ctx);
	
	cothread_restore_executor();
	
	
	
}
/* }}} */


/* 申请协程栈 */
/* {{{ cothread_vm_stack_new_page 
 */
static zend_always_inline zend_vm_stack cothread_vm_stack_new_page() {
	zend_vm_stack page = (zend_vm_stack)emalloc(COTHREAD_VM_STACK_INIT_SIZE);
	page->top = ZEND_VM_STACK_ELEMETS(page);
	page->end = (zval*)((char*)page + COTHREAD_VM_STACK_INIT_SIZE);
	page->prev = NULL;
	return page;
}
/* }}} */


/* 初始化协程上下文 */
/* {{{ cothread_init_context 
 */
static zend_always_inline void cothread_init_context(zend_object *object,cothread_context *ctx,zend_fcall_info_cache *fci_cache)
{
	ctx->shutdown_function_names = NULL;
	ctx->stack = cothread_vm_stack_new_page();
	ctx->status = COTHREAD_STATUS_SUSPEND; 

	ctx->top_execute_data = ctx->execute_data = NULL;

	ctx->this_obj = object;
	
	ctx->fci_cache = fci_cache;

	cothread_build_execute_data(ctx);
}
/* }}} */


/* 唤醒协程 */
/* {{{ resume_cothread 
 */
void resume_cothread(cothread_context *ctx)
{
	CO_G(context) = ctx;

	/* Backup executor globals */
	cothread_backup_executor();


	ctx->top_execute_data->prev_execute_data = EG(current_execute_data);
	ctx->status = COTHREAD_STATUS_RUNNING;

	
	/* Set executor globals */
	EG(current_execute_data) = ctx->execute_data;
	EG(scope) = ctx->execute_data->func->common.scope;
	EG(vm_stack_top) = ctx->stack->top;
	EG(vm_stack_end) = ctx->stack->end;
	EG(vm_stack) = ctx->stack;
	BG(user_shutdown_function_names)  = ctx->shutdown_function_names;


	
	



	/*** execute ***/
	zend_execute_ex(ctx->execute_data);
	



	if (ctx->status == COTHREAD_STATUS_RUNNING)
	{		
		ctx->execute_data = NULL;
		ctx->status = COTHREAD_STATUS_DEAD;
		php_call_shutdown_functions();
		php_free_shutdown_functions();
	}

	
	ctx->top_execute_data->prev_execute_data = NULL;

	
	ctx->stack = EG(vm_stack);
	ctx->stack->top = EG(vm_stack_top);
	ctx->shutdown_function_names = BG(user_shutdown_function_names);


	cothread_restore_executor();
	
	CO_G(context) = NULL;
}
/* }}} */



/* CoPHP class entry */
zend_class_entry *cothread_ce;


/**  CoPHP Method */

/* {{{ CoThread::__construct()
 */
ZEND_METHOD(cothread,__construct)
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

	//创建cothread执行上下文
	cothread_context *context = (cothread_context *)emalloc(sizeof(cothread_context));
	zend_update_property_long(cothread_ce ,getThis(),"context",7,(zend_long)context); 
	zend_update_property(cothread_ce ,getThis(),"callable",8,callback); 
	cothread_init_context(Z_OBJ_P(getThis()),context,fci_cache);
	

//	php_printf("created cothread context:%ld\n",context);
//	php_printf("OBJ=%ld\n",Z_OBJ_P(getThis()));


}
/* }}} */




/* {{{ CoThread::__construct()
 */
ZEND_METHOD(cothread,__destruct) 
{
	cothread_destory_context(Z_COTHREAD_CONTEXT_P(getThis()));
}
/* }}} */




/* {{{ CoThread::yield() 
 */
ZEND_METHOD(cothread,yield) 
{ 
	//php_printf("current_execute_data:%ld\n",EG(current_execute_data));


	if (CO_G(context)==NULL || CO_G(context)->status == COTHREAD_STATUS_SUSPEND)
	{
		RETURN_FALSE;
	}
	
	zend_op *next_op = (zend_op*)(OPLINE+1);
	CO_G(cache_handler) = (void*)next_op->handler;
	next_op->handler = COTHREAD_SUSPEND_HANDLER;
}
/* }}} */


/* {{{ CoThread::reset() 
 */
ZEND_METHOD(cothread,reset)
{
	if (CO_G(context) != NULL )
	{
		RETURN_NULL();
	}
	
	cothread_context * ctx = Z_COTHREAD_CONTEXT_P(getThis());
	
	if (ctx->status != COTHREAD_STATUS_DEAD)
	{
		RETURN_NULL();
	}

	cothread_build_execute_data(ctx);
	zend_update_property_long(cothread_ce,getThis(),"status",6,ctx->status);
}
/* }}} */


/* {{{ CoThread::running() 
 */
ZEND_METHOD(cothread,running)
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
ZEND_METHOD(cothread,resume)
{

	
	if (CO_G(context) != NULL)
	{
		RETURN_FALSE;
	}

	
	
	cothread_context * ctx = Z_COTHREAD_CONTEXT_P(getThis());

	
	if (ctx->status == COTHREAD_STATUS_DEAD)
	{
		RETURN_FALSE;
	}
	resume_cothread(ctx);
	zend_update_property_long(cothread_ce,getThis(),"status",6,ctx->status);
}
/* }}} */



/* CoThread class method entry */
/* {{{ cothread_method */
static zend_function_entry cothread_method[] = {
	ZEND_ME(cothread,  	yield						,  NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(cothread,  	running		,  NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(cothread,	__construct					,  NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	ZEND_ME(cothread,	__destruct					,  NULL,   ZEND_ACC_PUBLIC|ZEND_ACC_DTOR|ZEND_ACC_FINAL)
	ZEND_ME(cothread,	resume						,  NULL,   ZEND_ACC_PUBLIC)
	ZEND_ME(cothread,	reset						,  NULL,   ZEND_ACC_PUBLIC)

    { NULL, NULL, NULL }
};
/* }}} */


/* If you declare any globals in php_cophp.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(cophp)
*/

/* True global resources - no need for thread safety here */
static int le_cophp;

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("cophp.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_cophp_globals, cophp_globals)
    STD_PHP_INI_ENTRY("cophp.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_cophp_globals, cophp_globals)
PHP_INI_END()
*/
/* }}} */



/* {{{ php_cophp_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_cophp_init_globals(zend_cophp_globals *cophp_globals)
{
	cophp_globals->global_value = 0;
	cophp_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(cophp)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
	
	zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "CoThread",cothread_method);
    cothread_ce = zend_register_internal_class(&ce TSRMLS_CC);

	zend_declare_class_constant_long(cothread_ce, "STATUS_RUNNING", 	strlen("STATUS_RUNNING"),	COTHREAD_STATUS_RUNNING TSRMLS_CC);
	zend_declare_class_constant_long(cothread_ce, "STATUS_SUSPEND", 	strlen("STATUS_SUSPEND"),	COTHREAD_STATUS_SUSPEND TSRMLS_CC);
	zend_declare_class_constant_long(cothread_ce, "STATUS_DEAD", 		strlen("STATUS_DEAD"),		COTHREAD_STATUS_DEAD TSRMLS_CC);
	
    zend_declare_property_long(cothread_ce, "status", strlen("status"),COTHREAD_STATUS_SUSPEND, ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_long(cothread_ce, "context", strlen("context"),0, ZEND_ACC_PRIVATE TSRMLS_CC);
    zend_declare_property_long(cothread_ce, "callable", strlen("callable"),0, ZEND_ACC_PRIVATE TSRMLS_CC);

	
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(cophp)
{
	/* uncomment this line if you have INI entriescache_handler
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */




/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(cophp)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "cophp support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */



/* {{{ cophp_functions[]
 *
 * Every user visible function must have an entry in cophp_functions[].
 */
const zend_function_entry cophp_functions[] = {
	PHP_FE_END	/* Must be the last line in cophp_functions[] */
};
/* }}} */

/* {{{ cophp_module_entry
 */
zend_module_entry cophp_module_entry = {
	STANDARD_MODULE_HEADER,
	"cophp",
	cophp_functions,
	PHP_MINIT(cophp),
	PHP_MSHUTDOWN(cophp),
	NULL,//PHP_RINIT(cophp),		/* Replace with NULL if there's nothing to do at request start */
	NULL,//PHP_RSHUTDOWN(cophp),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(cophp),
	PHP_COPHP_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_COPHP
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif


/* {{{ zend_module */
//ZEND_DLEXPORT int cophp_zend_startup(zend_extension *extension)
//{
//	return zend_startup_module(&cophp_module_entry);
//}


//
//#ifndef ZEND_EXT_API
//#define ZEND_EXT_API    ZEND_DLEXPORT
//#endif

//ZEND_EXTENSION();

//ZEND_DLEXPORT zend_extension zend_extension_entry = {
//	"CoPHP",
//	"0.0.1",
//	"Blod Chen",
//	"",
//	"",
//	cophp_zend_startup,
//	NULL,
//	NULL,           /* activate_func_t */
//	NULL,           /* deactivate_func_t */
//	NULL,           /* message_handler_func_t */
//	NULL,           /* op_array_handler_func_t */
//	NULL, /* statement_handler_func_t */
//	NULL,           /* fcall_begin_handler_func_t */
//	NULL,           /* fcall_end_handler_func_t */
//	NULL,   /* op_array_ctor_func_t */
//	NULL,           /* op_array_dtor_func_t */
//	STANDARD_ZEND_EXTENSION_PROPERTIES
//};
/* }}} */


ZEND_GET_MODULE(cophp)
#endif




/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
