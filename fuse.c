/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Bob Carroll <rjcarroll@php.net>                              |
  +----------------------------------------------------------------------+
*/

/* $Id: fuse.c 290089 2009-10-30 19:35:58Z rjcarroll $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_fuse.h"
#include <sys/time.h>

/* True global resources - no need for thread safety here */
static int le_fuse;


/* {{{ fuse_functions[]
 *
 * Every user visible function must have an entry in fuse_functions[].
 */
zend_function_entry fuse_functions[] = {
	PHP_FE(fuse_main,	NULL)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ foper_functions[]
 */
static zend_function_entry foper_functions[] = {
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ fstat_functions[]
 */
static zend_function_entry fstat_functions[] = {
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ finfo_functions[]
 */
static zend_function_entry finfo_functions[] = {
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ fwrapper_functions[]
 */
static zend_function_entry fwrapper_functions[] = {
	PHP_ME(FuseWrapper, Init, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_FINAL)
	PHP_ME(FuseWrapper, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(FuseWrapper, PrintVersion, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC|ZEND_ACC_FINAL)
	PHP_MALIAS(FuseWrapper, FsGetAttr, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_MALIAS(FuseWrapper, FsOpen, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_MALIAS(FuseWrapper, FsRead, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_MALIAS(FuseWrapper, FsReadDir, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_MALIAS(FuseWrapper, FsReadLink, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_MALIAS(FuseWrapper, FsUnlink, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_MALIAS(FuseWrapper, FsWrite, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_MALIAS(FuseWrapper, FsUtimens, NotImpl, NULL, ZEND_ACC_PUBLIC)
	PHP_MALIAS(FuseWrapper, FsCreate, NotImpl, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ fuse_module_entry
 */
zend_module_entry fuse_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"fuse",
	fuse_functions,
	PHP_MINIT(fuse),
	PHP_MSHUTDOWN(fuse),
	NULL,
	NULL,
	PHP_MINFO(fuse),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1",
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */


#ifdef COMPILE_DL_FUSE
ZEND_GET_MODULE(fuse)
#endif


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(fuse)
{
	register_errno_constants(module_number);

	init_fuse_operations();
	init_fuse_stat(module_number);
	init_fuse_fileinfo();
	init_fuse_filetime();

	init_fuse_wrapper();

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(fuse)
{
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(fuse)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "fuse support", "enabled");
	php_info_print_table_end();
}
/* }}} */


/* {{{ proto void fuse_main(array argv, FuseOperations op)
   Main FUSE function (for the lazy) that parses command line options and mounts the target file system. */
PHP_FUNCTION(fuse_main)
{
	int argc, i = 0;
	zval* pzargv;
	zval* pzop;
	zval* pzud;
	char** ppchargv;
	zval** ppzargitm;
	struct fuse_operations op;

	if(zend_parse_parameters(
				ZEND_NUM_ARGS() TSRMLS_CC, 
				"aO|z", 
				&pzargv, 
				&pzop, 
				foper_ce_ptr, 
				&pzud
			) != SUCCESS)
		return;

	convert_to_array_ex(&pzargv);
	argc = zend_hash_num_elements(Z_ARRVAL_P(pzargv));
	if(argc == 1) {
		add_index_string(pzargv, argc, "-h", 1);
		argc++;
	}

#ifndef ZTS
	/* Force FUSE callbacks to be single-threaded */
	add_index_string(pzargv, argc, "-s", 1);
	argc++;
#endif

	ppchargv = malloc(sizeof(char*) * argc);
	zend_hash_internal_pointer_reset(Z_ARRVAL_P(pzargv));
	while(zend_hash_get_current_data(Z_ARRVAL_P(pzargv), (void**) &ppzargitm) == SUCCESS) {
		if(i == argc) break;

		ppchargv[i] = Z_STRVAL_PP(ppzargitm);
		i++;

		zend_hash_move_forward(Z_ARRVAL_P(pzargv));
	}

	client_oper_ptr = pzop;

	memset(&op, 0, sizeof(struct fuse_operations));
	op.getattr  = fuse_getattr_cb;
	op.open     = fuse_open_cb;
	op.read     = fuse_read_cb;
	op.readdir  = fuse_readdir_cb;
	op.readlink = fuse_readlink_cb;
	op.write    = fuse_write_cb;
	op.unlink   = fuse_unlink_cb;
	op.utimens  = fuse_utimens_cb;
	op.create   = fuse_create_cb;

	fuse_main(argc, ppchargv, &op, NULL);

	free(ppchargv);
}
/* }}} */


/* {{{ fuse_getattr_cb
 */
static int fuse_getattr_cb(const char* path, struct stat* stbuf)
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[2];
	int errno;
	int rc;
	TSRMLS_FETCH();

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("getattr") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	MAKE_STD_ZVAL(pzargs[1]);
	object_init_ex(pzargs[1], fstat_ce_ptr);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 2, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		zval_ptr_dtor(&pzargs[0]);
		zval_ptr_dtor(&pzargs[1]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_dev     = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_dev") - 1, 1 TSRMLS_CC));
	stbuf->st_ino     = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_ino") - 1, 1 TSRMLS_CC));
	stbuf->st_mode    = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_mode") - 1, 1 TSRMLS_CC));
	stbuf->st_nlink   = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_nlink") - 1, 1 TSRMLS_CC));
	stbuf->st_uid     = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_uid") - 1, 1 TSRMLS_CC));
	stbuf->st_gid     = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_gid") - 1, 1 TSRMLS_CC));
	stbuf->st_rdev    = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_rdev") - 1, 1 TSRMLS_CC));
	stbuf->st_size    = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_size") - 1, 1 TSRMLS_CC));
	stbuf->st_atime   = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_atime") - 1, 1 TSRMLS_CC));
	stbuf->st_mtime   = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_mtime") - 1, 1 TSRMLS_CC));
	stbuf->st_ctime   = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_ctime") - 1, 1 TSRMLS_CC));
	stbuf->st_blksize = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("st_blksize") - 1, 1 TSRMLS_CC));

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;

	zval_ptr_dtor(&pzargs[0]);
	zval_ptr_dtor(&pzargs[1]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ fuse_utimens_cb
 */
static int fuse_utimens_cb(const char* path, const struct timespec ts[2])
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[2];
	int errno;
	int rc;
	TSRMLS_FETCH();

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("utimens") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	MAKE_STD_ZVAL(pzargs[1]);
	object_init_ex(pzargs[1], ftime_ce_ptr);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 2, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		zval_ptr_dtor(&pzargs[0]);
		zval_ptr_dtor(&pzargs[1]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;

	zval_ptr_dtor(&pzargs[0]);
	zval_ptr_dtor(&pzargs[1]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ fuse_unlink_cb
 */
static int fuse_unlink_cb(const char* path)
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[1];
	int errno;
	int rc;
	TSRMLS_FETCH();

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("unlink") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 1, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		zval_ptr_dtor(&pzargs[0]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;

	zval_ptr_dtor(&pzargs[0]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ fuse_create_cb
 */
static int fuse_create_cb(const char* path, mode_t mode, struct fuse_file_info* fi)
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[3];
	int errno;
	int rc;
	TSRMLS_FETCH();

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("create") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	MAKE_STD_ZVAL(pzargs[1]);
	object_init_ex(pzargs[1], fstat_ce_ptr);

	MAKE_STD_ZVAL(pzargs[2]);
	object_init_ex(pzargs[2], finfo_ce_ptr);

	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("flags") - 1, fi->flags TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("fh_old") - 1, fi->fh_old TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("writepage") - 1, fi->writepage TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("direct_io") - 1, fi->direct_io TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("keep_cache") - 1, fi->keep_cache TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("flush") - 1, fi->flush TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("padding") - 1, fi->padding TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("fh") - 1, fi->fh TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("lock_owner") - 1, fi->lock_owner TSRMLS_CC);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 3, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		zval_ptr_dtor(&pzargs[0]);
		zval_ptr_dtor(&pzargs[1]);
		zval_ptr_dtor(&pzargs[2]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	fi->flags      = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("flags") - 1, 1 TSRMLS_CC));
	fi->fh_old     = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("padding") - 1, 1 TSRMLS_CC));
	fi->writepage  = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("writepage") - 1, 1 TSRMLS_CC));
	fi->direct_io  = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("direct_io") - 1, 1 TSRMLS_CC));
	fi->keep_cache = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("keep_cache") - 1, 1 TSRMLS_CC));
	fi->flush      = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("flush") - 1, 1 TSRMLS_CC));
	fi->padding    = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("padding") - 1, 1 TSRMLS_CC));
	fi->fh         = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("fh") - 1, 1 TSRMLS_CC));
	fi->lock_owner = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[2]), pzargs[2], ZEND_STRS("lock_owner") - 1, 1 TSRMLS_CC));

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;

	zval_ptr_dtor(&pzargs[0]);
	zval_ptr_dtor(&pzargs[1]);
	zval_ptr_dtor(&pzargs[2]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ fuse_open_cb
 */
static int fuse_open_cb(const char* path, struct fuse_file_info* fi)
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[2];
	int errno;
	int rc;
	TSRMLS_FETCH();

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("open") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	MAKE_STD_ZVAL(pzargs[1]);
	object_init_ex(pzargs[1], finfo_ce_ptr);

	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("flags") - 1, fi->flags TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("fh_old") - 1, fi->fh_old TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("writepage") - 1, fi->writepage TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("direct_io") - 1, fi->direct_io TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("keep_cache") - 1, fi->keep_cache TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("flush") - 1, fi->flush TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("padding") - 1, fi->padding TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("fh") - 1, fi->fh TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("lock_owner") - 1, fi->lock_owner TSRMLS_CC);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 2, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		zval_ptr_dtor(&pzargs[0]);
		zval_ptr_dtor(&pzargs[1]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	fi->flags      = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("flags") - 1, 1 TSRMLS_CC));
	fi->fh_old     = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("padding") - 1, 1 TSRMLS_CC));
	fi->writepage  = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("writepage") - 1, 1 TSRMLS_CC));
	fi->direct_io  = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("direct_io") - 1, 1 TSRMLS_CC));
	fi->keep_cache = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("keep_cache") - 1, 1 TSRMLS_CC));
	fi->flush      = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("flush") - 1, 1 TSRMLS_CC));
	fi->padding    = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("padding") - 1, 1 TSRMLS_CC));
	fi->fh         = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("fh") - 1, 1 TSRMLS_CC));
	fi->lock_owner = Z_LVAL_P(zend_read_property(Z_OBJCE_P(pzargs[1]), pzargs[1], ZEND_STRS("lock_owner") - 1, 1 TSRMLS_CC));

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;

	zval_ptr_dtor(&pzargs[0]);
	zval_ptr_dtor(&pzargs[1]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ fuse_read_cb
 */
static int fuse_read_cb(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[5];
	int errno;
	int rc, i;
	TSRMLS_FETCH();

	memset(buf, 0, size);

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("read") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	MAKE_STD_ZVAL(pzargs[1]);
	ZVAL_STRINGL(pzargs[1], (char*) buf, size, 1);

	MAKE_STD_ZVAL(pzargs[2]);
	ZVAL_LONG(pzargs[2], size);

	MAKE_STD_ZVAL(pzargs[3]);
	ZVAL_LONG(pzargs[3], offset);

	MAKE_STD_ZVAL(pzargs[4]);
	object_init_ex(pzargs[4], finfo_ce_ptr);

	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("flags") - 1, fi->flags TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("fh_old") - 1, fi->fh_old TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("writepage") - 1, fi->writepage TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("direct_io") - 1, fi->direct_io TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("keep_cache") - 1, fi->keep_cache TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("flush") - 1, fi->flush TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("padding") - 1, fi->padding TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("fh") - 1, fi->fh TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("lock_owner") - 1, fi->lock_owner TSRMLS_CC);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 5, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		for(i = 0; i < 5; i++)
			zval_ptr_dtor(&pzargs[i]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;
	if(rc < 0) return rc;

	if(rc > size) rc = size;
	memcpy(buf, Z_STRVAL_P(pzargs[1]), rc);

	for(i = 0; i < 5; i++)
		zval_ptr_dtor(&pzargs[i]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ fuse_readdir_cb
 */
static int fuse_readdir_cb(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[2];
	zval** ppzname;
	int entryct;
	int errno;
	int rc;
	TSRMLS_FETCH();

	(void) offset;
	(void) fi;

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("readdir") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	MAKE_STD_ZVAL(pzargs[1]);
	array_init(pzargs[1]);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 2, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		zval_ptr_dtor(&pzargs[0]);
		zval_ptr_dtor(&pzargs[1]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	convert_to_array_ex(&pzargs[1]);
	entryct = zend_hash_num_elements(Z_ARRVAL_P(pzargs[1]));
	zend_hash_internal_pointer_reset(Z_ARRVAL_P(pzargs[1]));
	while(zend_hash_get_current_data(Z_ARRVAL_P(pzargs[1]), (void**) &ppzname) == SUCCESS) {
		filler(buf, Z_STRVAL_PP(ppzname), NULL, 0);
		zend_hash_move_forward(Z_ARRVAL_P(pzargs[1]));
	}

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;

	zval_ptr_dtor(&pzargs[0]);
	zval_ptr_dtor(&pzargs[1]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ fuse_readlink_cb
 */
static int fuse_readlink_cb(const char* path, char* buf, size_t size)
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[3];
	int errno;
	int rc, i;
	TSRMLS_FETCH();

	memset(buf, 0, size);

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("readlink") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	MAKE_STD_ZVAL(pzargs[1]);
	ZVAL_STRINGL(pzargs[1], (char*) buf, size, 1);

	MAKE_STD_ZVAL(pzargs[2]);
	ZVAL_LONG(pzargs[2], size);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 3, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		for(i = 0; i < 3; i++)
			zval_ptr_dtor(&pzargs[i]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;
	if(rc < 0) return rc;

	if(rc > size) rc = size;
	memcpy(buf, Z_STRVAL_P(pzargs[1]), size);

	for(i = 0; i < 3; i++)
		zval_ptr_dtor(&pzargs[i]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ fuse_write_cb
 */
static int fuse_write_cb(const char* path, const char* buf, size_t len, off_t offset, struct fuse_file_info* fi)
{
	zval* pzcbname;
	zval zretval;
	zval* pzargs[5];
	int errno;
	int rc, i;
	TSRMLS_FETCH();

	pzcbname = zend_read_property(Z_OBJCE_P(client_oper_ptr), client_oper_ptr, ZEND_STRS("write") - 1, 1 TSRMLS_CC);
	if(Z_STRVAL_P(pzcbname) == NULL) return -ENOSYS;

	MAKE_STD_ZVAL(pzargs[0]);
	ZVAL_STRINGL(pzargs[0], (char*) path, strlen(path), 1);

	MAKE_STD_ZVAL(pzargs[1]);
	ZVAL_STRINGL(pzargs[1], (char*) buf, len, 1);

	MAKE_STD_ZVAL(pzargs[2]);
	ZVAL_LONG(pzargs[2], len);

	MAKE_STD_ZVAL(pzargs[3]);
	ZVAL_LONG(pzargs[3], offset);

	MAKE_STD_ZVAL(pzargs[4]);
	object_init_ex(pzargs[4], finfo_ce_ptr);

	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("flags") - 1, fi->flags TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("fh_old") - 1, fi->fh_old TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("writepage") - 1, fi->writepage TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("direct_io") - 1, fi->direct_io TSRMLS_CC);
	TSRMLS_DCzend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("keep_cache") - 1, fi->keep_cache TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("flush") - 1, fi->flush TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("padding") - 1, fi->padding TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("fh") - 1, fi->fh TSRMLS_CC);
	zend_update_property_long(Z_OBJCE_P(pzargs[4]), pzargs[4], ZEND_STRS("lock_owner") - 1, fi->lock_owner TSRMLS_CC);

	errno = call_user_function(EG(function_table), NULL, pzcbname, (zval*) &zretval, 5, pzargs TSRMLS_CC);
	if(errno != SUCCESS) {
		for(i = 0; i < 5; i++)
			zval_ptr_dtor(&pzargs[i]);

		PHPFUSE_CALL_ERROR(pzcbname);
		return -ENOSYS;
	}

	rc = Z_TYPE(zretval) == IS_LONG ? Z_LVAL(zretval) : 0;
	if(rc < 0) return rc;

	if(rc > len) rc = len;

	for(i = 0; i < 5; i++)
		zval_ptr_dtor(&pzargs[i]);
	zval_dtor(&zretval);

	return rc;
}
/* }}} */


/* {{{ register_errno_constants()
 */
static void register_errno_constants(int module_number)
{
	TSRMLS_FETCH();

	zend_register_long_constant(ZEND_STRS("FUSE_ENOERR"), 0, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EPERM"), EPERM, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOENT"), ENOENT, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ESRCH"), ESRCH, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EINTR"), EINTR, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EIO"), EIO, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENXIO"), ENXIO, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_E2BIG"), E2BIG, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOEXEC"), ENOEXEC, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EBADF"), EBADF, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ECHILD"), ECHILD, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EDEADLK"), EDEADLK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOMEM"), ENOMEM, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EACCES"), EACCES, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EFAULT"), EFAULT, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOTBLK"), ENOTBLK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EBUSY"), EBUSY, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EEXIST"), EEXIST, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EXDEV"), EXDEV, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENODEV"), ENODEV, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOTDIR"), ENOTDIR, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EISDIR"), EISDIR, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EINVAL"), EINVAL, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENFILE"), ENFILE, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EMFILE"), EMFILE, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOTTY"), ENOTTY, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ETXTBSY"), ETXTBSY, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EFBIG"), EFBIG, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOSPC"), ENOSPC, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ESPIPE"), ESPIPE, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EROFS"), EROFS, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EMLINK"), EMLINK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EPIPE"), EPIPE, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EDEADLK"), EDEADLK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENAMETOOLONG"), ENAMETOOLONG, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOLCK"), ENOLCK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOSYS"), ENOSYS, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ENOTEMPTY"), ENOTEMPTY, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_ELOOP"), ELOOP, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_EWOULDBLOCK"), EAGAIN, CONST_CS, module_number TSRMLS_CC);
}
/* }}} */


/* {{{ init_fuse_operations()
 */
static void init_fuse_operations()
{
	zend_class_entry ce;
	TSRMLS_FETCH();

	INIT_CLASS_ENTRY(ce, "FuseOperations", foper_functions);
	foper_ce_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_string(foper_ce_ptr, ZEND_STRS("getattr"), "", ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_string(foper_ce_ptr, ZEND_STRS("readdir"), "", ZEND_ACC_PUBLIC TSRMLS_CC);
}
/* }}} */


/* {{{ init_fuse_stat()
 */
static void init_fuse_stat(int module_number)
{
	zend_class_entry ce;
	TSRMLS_FETCH();

	INIT_CLASS_ENTRY(ce, "FuseStat", fstat_functions);
	fstat_ce_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_dev"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_ino"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_mode"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_nlink"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_uid"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_gid"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_rdev"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_size"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_atime"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_mtime"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_ctime"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(fstat_ce_ptr, ZEND_STRS("st_blksize"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);

	zend_register_long_constant(ZEND_STRS("FUSE_S_IFBLK"), S_IFBLK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IFCHR"), S_IFCHR, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IFDIR"), S_IFDIR, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IFIFO"), S_IFIFO, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IFLNK"), S_IFLNK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IFMT"), S_IFMT, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IFREG"), S_IFREG, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IFSOCK"), S_IFSOCK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IRGRP"), S_IRGRP, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IROTH"), S_IROTH, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IRUSR"), S_IRUSR, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_ISGID"), S_ISGID, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_ISUID"), S_ISUID, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IWGRP"), S_IWGRP, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IWOTH"), S_IWOTH, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IWUSR"), S_IWUSR, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IXGRP"), S_IXGRP, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IXOTH"), S_IXOTH, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_S_IXUSR"), S_IXUSR, CONST_CS, module_number TSRMLS_CC);

	zend_register_long_constant(ZEND_STRS("FUSE_O_ACCMODE"), O_ACCMODE, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_RDONLY"), O_RDONLY, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_WRONLY"), O_WRONLY, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_RDWR"), O_RDWR, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_CREAT"), O_CREAT, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_EXCL"), O_EXCL, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_NOCTTY"), O_NOCTTY, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_TRUNC"), O_TRUNC, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_APPEND"), O_APPEND, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_NONBLOCK"), O_NONBLOCK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_NDELAY"), O_NONBLOCK, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_SYNC"), O_SYNC, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_DIRECTORY"), O_DIRECTORY, CONST_CS, module_number TSRMLS_CC);
	zend_register_long_constant(ZEND_STRS("FUSE_O_NOFOLLOW"), O_NOFOLLOW, CONST_CS, module_number TSRMLS_CC);
}
/* }}} */


/* {{{ init_fuse_fileinfo()
 */
static void init_fuse_fileinfo()
{
	zend_class_entry ce;
	TSRMLS_FETCH();

	INIT_CLASS_ENTRY(ce, "FuseFileInfo", finfo_functions);
	finfo_ce_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("flags"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("fh_old"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("writepage"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("direct_io"), 1, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("keep_cache"), 1, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("flush"), 1, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("padding"), 29, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("fh"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(finfo_ce_ptr, ZEND_STRS("lock_owner"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
}
/* }}} */

/* {{{ init_fuse_filetime()
 */
static void init_fuse_filetime()
{
	zend_class_entry ce;
	TSRMLS_FETCH();

	INIT_CLASS_ENTRY(ce, "FuseFileTime", finfo_functions);
	ftime_ce_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(ftime_ce_ptr, ZEND_STRS("tv_sec"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(ftime_ce_ptr, ZEND_STRS("tv_nsec"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
}
/* }}} */

/* {{{ init_fuse_wrapper()
 */
static void init_fuse_wrapper()
{
	zend_class_entry ce;
	TSRMLS_FETCH();

	INIT_CLASS_ENTRY(ce, "FuseWrapper", fwrapper_functions);
	fwrapper_ce_ptr = zend_register_internal_class(&ce TSRMLS_CC);
	fwrapper_ce_ptr->ce_flags |= ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;
}
/* }}} */


/* {{{ update_callback_table()
 */
static void update_callback_table(char* pchfn, zval* pzinst, zval* pzop, char* pchprop)
{	
	zval* pzarray;
	TSRMLS_FETCH();

	MAKE_STD_ZVAL(pzarray);
	array_init(pzarray);

	add_next_index_zval(pzarray, pzinst);
	add_next_index_string(pzarray, pchfn, 1);

	zend_update_property(Z_OBJCE_P(pzop), pzop, pchprop, strlen(pchprop), pzarray TSRMLS_CC);

	zval_ptr_dtor(&pzarray);
}
/* }}} */


/* {{{ proto void Init(array argv)
   Initializes the FUSE file system. */
PHP_METHOD(FuseWrapper, Init)
{
	zval* pzargs[2];
	zval* pzfn;
	zval zretval;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &pzargs[0]) != SUCCESS)
		return;

	MAKE_STD_ZVAL(pzargs[1]);
	object_init_ex(pzargs[1], foper_ce_ptr);

	update_callback_table("FsGetAttr",  getThis(), pzargs[1], "getattr");
	update_callback_table("FsOpen",     getThis(), pzargs[1], "open");
	update_callback_table("FsRead",     getThis(), pzargs[1], "read");
	update_callback_table("FsReadDir",  getThis(), pzargs[1], "readdir");
	update_callback_table("FsReadLink", getThis(), pzargs[1], "readlink");
	update_callback_table("FsWrite",    getThis(), pzargs[1], "write");
	update_callback_table("FsUnlink",   getThis(), pzargs[1], "unlink");
	update_callback_table("FsUtimens",  getThis(), pzargs[1], "utimens");
	update_callback_table("FsCreate",   getThis(), pzargs[1], "create");

	MAKE_STD_ZVAL(pzfn);
	ZVAL_STRING(pzfn, "fuse_main", 1);

	call_user_function(EG(function_table), NULL, pzfn, (zval*) &zretval, 2, pzargs TSRMLS_CC);

	zval_ptr_dtor(&pzargs[1]);
	zval_ptr_dtor(&pzfn);
	zval_dtor(&zretval);
}
/* }}} */


/* {{{ proto void PrintVersion()
   Prints the FUSE version to STDOUT. */
PHP_METHOD(FuseWrapper, PrintVersion)
{
	char* ppchargv[] = { "php", "--version" };
	struct fuse_operations op;

	memset(&op, 0, sizeof(struct fuse_operations));
	fuse_main( 2, ppchargv, &op, NULL );
}
/* }}} */


/* {{{ proto void NotImpl()
   Dummy function that does not nothing (returns ENOSYS). */
PHP_METHOD(FuseWrapper, NotImpl)
{
	RETURN_LONG(-ENOSYS);
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
