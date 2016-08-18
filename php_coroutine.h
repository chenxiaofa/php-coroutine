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

#ifndef PHP_COROUTINE_H
#define PHP_COROUTINE_H

extern zend_module_entry coroutine_module_entry;
#define phpext_coroutine_ptr &coroutine_module_entry

#define PHP_COROUTINE_VERSION "0.1.0" /* Replace with version number for your extension */

#define COROUTINE_VM_STACK_INIT_SIZE 512

#ifdef PHP_WIN32
#	define PHP_COROUTINE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_COROUTINE_API __attribute__ ((visibility("default")))
#else
#	define PHP_COROUTINE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define Z_COROUTINE_CONTEXT_P(z) ((coroutine_context *)(zend_read_property(coroutine_ce,(z),"context",7,1,NULL)->value.lval))
#define COROUTINE_SYMBOL_TABLE(ctx) (ctx)->symbol_table
#define COROUTINE_SYMBOL_TABLE_P(ctx) &(COROUTINE_SYMBOL_TABLE(ctx))
#define CO_G(v) coroutine_globals.v
#define CURRCO(v) CO_G(context)->v

static zend_always_inline void i_init_execute_data(zend_execute_data *execute_data, zend_op_array *op_array, zval *return_value) /* {{{ */
{


	EX(opline) = op_array->opcodes;
	EX(call) = NULL;
	EX(return_value) = return_value;

	if (UNEXPECTED(EX(symbol_table) != NULL)) {
		if (UNEXPECTED(op_array->this_var != (uint32_t)-1) && EXPECTED(Z_OBJ(EX(This)))) {
			GC_REFCOUNT(Z_OBJ(EX(This)))++;
			if (!zend_hash_str_add(EX(symbol_table), "this", sizeof("this")-1, &EX(This))) {
				GC_REFCOUNT(Z_OBJ(EX(This)))--;
			}
		}

		zend_attach_symbol_table(execute_data);
	} else {
		uint32_t first_extra_arg, num_args;

		/* Handle arguments */
		first_extra_arg = op_array->num_args;
		num_args = EX_NUM_ARGS();
		if (UNEXPECTED(num_args > first_extra_arg)) {
			if (EXPECTED(!(op_array->fn_flags & ZEND_ACC_CALL_VIA_TRAMPOLINE))) {
				zval *end, *src, *dst;
				uint32_t type_flags = 0;

				if (EXPECTED((op_array->fn_flags & ZEND_ACC_HAS_TYPE_HINTS) == 0)) {
					/* Skip useless ZEND_RECV and ZEND_RECV_INIT opcodes */
					EX(opline) += first_extra_arg;
				}

				/* move extra args into separate array after all CV and TMP vars */
				end = EX_VAR_NUM(first_extra_arg - 1);
				src = end + (num_args - first_extra_arg);
				dst = src + (op_array->last_var + op_array->T - first_extra_arg);
				if (EXPECTED(src != dst)) {
					do {
						type_flags |= Z_TYPE_INFO_P(src);
						ZVAL_COPY_VALUE(dst, src);
						ZVAL_UNDEF(src);
						src--;
						dst--;
					} while (src != end);
				} else {
					do {
						type_flags |= Z_TYPE_INFO_P(src);
						src--;
					} while (src != end);
				}
				ZEND_ADD_CALL_FLAG(execute_data, ((type_flags >> Z_TYPE_FLAGS_SHIFT) & IS_TYPE_REFCOUNTED));
			}
		} else if (EXPECTED((op_array->fn_flags & ZEND_ACC_HAS_TYPE_HINTS) == 0)) {
			/* Skip useless ZEND_RECV and ZEND_RECV_INIT opcodes */
			EX(opline) += num_args;
		}

		/* Initialize CV variables (skip arguments) */
		if (EXPECTED((int)num_args < op_array->last_var)) {
			zval *var = EX_VAR_NUM(num_args);
			zval *end = EX_VAR_NUM(op_array->last_var);

			do {
				ZVAL_UNDEF(var);
				var++;
			} while (var != end);
		}

		if (op_array->this_var != (uint32_t)-1 && EXPECTED(Z_OBJ(EX(This)))) {
			ZVAL_OBJ(EX_VAR(op_array->this_var), Z_OBJ(EX(This)));
			GC_REFCOUNT(Z_OBJ(EX(This)))++;
		}
	}

	if (!op_array->run_time_cache) {
		if (op_array->function_name) {
			op_array->run_time_cache = zend_arena_alloc(&CG(arena), op_array->cache_size);
		} else {
			op_array->run_time_cache = emalloc(op_array->cache_size);
		}
		memset(op_array->run_time_cache, 0, op_array->cache_size);
	}
	EX_LOAD_RUN_TIME_CACHE(op_array);
	EX_LOAD_LITERALS(op_array);

	EG(current_execute_data) = execute_data;
	//ZEND_VM_INTERRUPT_CHECK();
}
/* }}} */

struct _coroutine_function_hook{
	void* raw_ptr;
	zend_function *target_function;
	zend_fcall_info fci;
	zend_fcall_info_cache fci_cache;
};

typedef struct _coroutine_function_hook coroutine_function_hook;


struct _coroutine_context{
	/* CoThread instance */
	zend_object* this_obj;
	
	/* The suspended execution context. */
	zend_execute_data *execute_data;
	
	zend_execute_data *top_execute_data;
	/* The separate stack used by coroutine */
	zend_vm_stack stack;

	zend_fcall_info_cache *fci_cache;


	char status;

	HashTable  *shutdown_function_names;
	HashTable  *function_hook;

};

typedef struct _coroutine_context coroutine_context;

struct _coroutine_globals{
	coroutine_context 	*context;
	zend_execute_data 	*main_execute_data;
	zend_class_entry 	*main_scope;
	zend_vm_stack 		 main_stack;
	void				*cache_handler;
	HashTable			*shutdown_function_names;

} coroutine_globals;



/*
  	Declare any global variables you may need between the BEGIN
	and END macros here:

ZEND_BEGIN_MODULE_GLOBALS(coroutine)
	zend_long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(coroutine)
*/

/* Always refer to the globals in your function as COROUTINE_G(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/
#define COROUTINE_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(coroutine, v)

#if defined(ZTS) && defined(COMPILE_DL_COROUTINE)
ZEND_TSRMLS_CACHE_EXTERN()
#endif




#ifdef HAVE_GCC_GLOBAL_REGS
# if defined(__GNUC__) && ZEND_GCC_VERSION >= 4008 && defined(i386)
#  define ZEND_VM_FP_GLOBAL_REG "%esi"
#  define ZEND_VM_IP_GLOBAL_REG "%edi"
# elif defined(__GNUC__) && ZEND_GCC_VERSION >= 4008 && defined(__x86_64__)
#  define ZEND_VM_FP_GLOBAL_REG "%r14"
#  define ZEND_VM_IP_GLOBAL_REG "%r15"
# elif defined(__GNUC__) && ZEND_GCC_VERSION >= 4008 && defined(__powerpc64__)
#  define ZEND_VM_FP_GLOBAL_REG "r28"
#  define ZEND_VM_IP_GLOBAL_REG "r29"
# elif defined(__IBMC__) && ZEND_GCC_VERSION >= 4002 && defined(__powerpc64__)
#  define ZEND_VM_FP_GLOBAL_REG "r28"
#  define ZEND_VM_IP_GLOBAL_REG "r29"
# endif
#endif

#ifdef ZEND_VM_IP_GLOBAL_REG
#pragma GCC diagnostic ignored "-Wvolatile-register-var"
register const zend_op* volatile opline __asm__(ZEND_VM_IP_GLOBAL_REG);
#pragma GCC diagnostic warning "-Wvolatile-register-var"
#endif

#ifdef ZEND_VM_FP_GLOBAL_REG
#pragma GCC diagnostic ignored "-Wvolatile-register-var"
register zend_execute_data* volatile execute_data __asm__(ZEND_VM_FP_GLOBAL_REG);
#pragma GCC diagnostic warning "-Wvolatile-register-var"
#endif


#ifdef ZEND_VM_IP_GLOBAL_REG
# define OPLINE opline
# define USE_OPLINE
# define LOAD_OPLINE() opline = EX(opline)
# define LOAD_NEXT_OPLINE() opline = EX(opline) + 1
# define SAVE_OPLINE() EX(opline) = opline
#else
# define OPLINE EX(opline)
# define USE_OPLINE const zend_op *opline = EX(opline);
# define LOAD_OPLINE()
# define LOAD_NEXT_OPLINE() ZEND_VM_INC_OPCODE()
# define SAVE_OPLINE()
#endif




#endif	/* PHP_COROUTINE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
