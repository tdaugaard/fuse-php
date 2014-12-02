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

/* $Id: php_fuse.h 270734 2008-12-07 03:46:34Z rjcarroll $ */

#ifndef PHP_FUSE_H
#define PHP_FUSE_H

extern zend_module_entry fuse_module_entry;
#define phpext_fuse_ptr &fuse_module_entry

#ifdef PHP_WIN32
#define PHP_FUSE_API __declspec(dllexport)
#else
#define PHP_FUSE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#ifdef __Darwin__		/* FUSE needs this, but Zend/zend.c won't  */
#define __FreeBSD__ 10		/* compile on Darwin with FreeBSD defined. */
#endif				/* I think this is better anyway...        */

#include "fuse.h"
#include <sys/time.h>

#define PHPFUSE_CALL_ERROR(pzv) \
	zend_error(E_WARNING, "Could not call user-space function %s!", Z_STRVAL_P(pzv));

PHP_MINIT_FUNCTION(fuse);
PHP_MSHUTDOWN_FUNCTION(fuse);
PHP_MINFO_FUNCTION(fuse);

PHP_FUNCTION(fuse_main);

zval* client_oper_ptr;

static int fuse_getattr_cb(const char*, struct stat*);
static int fuse_open_cb(const char*, struct fuse_file_info*);
static int fuse_read_cb(const char*, char*, size_t, off_t, struct fuse_file_info*);
static int fuse_readdir_cb(const char*, void*, fuse_fill_dir_t, off_t, struct fuse_file_info*);
static int fuse_readlink_cb(const char*, char*, size_t);
static int fuse_write_cb(const char*, const char*, size_t, off_t, struct fuse_file_info*);
static int fuse_utimens_cb(const char* path, const struct timespec ts[2]);
static int fuse_unlink_cb(const char*);
static int fuse_create_cb(const char*, mode_t, struct fuse_file_info*);
static void register_errno_constants(int);

zend_class_entry* foper_ce_ptr;
static void init_fuse_operations();

zend_class_entry* fstat_ce_ptr;
static void init_fuse_stat(int);

zend_class_entry* finfo_ce_ptr;
static void init_fuse_fileinfo();

zend_class_entry* ftime_ce_ptr;
static void init_fuse_filetime();

zend_class_entry* fwrapper_ce_ptr;
static void init_fuse_wrapper();
static void update_callback_table(char*, zval*, zval*, char*);
PHP_METHOD(FuseWrapper, Init);
PHP_METHOD(FuseWrapper, NotImpl);
PHP_METHOD(FuseWrapper, PrintVersion);

#ifdef ZTS
#define FUSE_G(v) TSRMG(fuse_globals_id, zend_fuse_globals *, v)
#else
#define FUSE_G(v) (fuse_globals.v)
#endif

#endif	/* PHP_FUSE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
