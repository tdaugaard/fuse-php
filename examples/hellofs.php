<?php

if( !extension_loaded("fuse") )
	dl( "fuse." . PHP_SHLIB_SUFFIX );

$op = new FuseOperations();
$op->getattr = "hello_getattr";
$op->open = "hello_open";
$op->read = "hello_read";
$op->readdir = "hello_readdir";

fuse_main( $argv, $op );


function hello_getattr( $path, $stbuf )
{
	if( strcmp($path, "/") == 0 ) {
		$stbuf->st_mode = FUSE_S_IFDIR | 0755;
		$stbuf->st_nlink = 2;
	} else {
		$stbuf->st_uid = posix_getuid();
		$stbuf->st_gid = posix_getgid();
		$stbuf->st_mode = FUSE_S_IFREG | 0644;
		$stbuf->st_nlink = 1;
		$stbuf->st_size = 56;
		$stbuf->st_mtime = 1226379246;
	}

	return -FUSE_ENOERR;
}

function hello_open( $path, $fi )
{
	if( ($fi->flags & FUSE_O_ACCMODE) != FUSE_O_RDONLY )
		return -FUSE_ENOACCES;

	return -FUSE_ENOERR;
}

function hello_read( $path, &$buf, $size, $offset, $fi )
{
	$data = "You've implemented your first FUSE file system with PHP!";

	if( $offset > strlen($data) )
		return 0;

	$buf = substr( $data, $offset, $size );

	return strlen($data);
}

function hello_readdir( $path, &$buf )
{
	if( strcmp($path, "/") != 0 )
		return -FUSE_ENOENT;

	$buf[] = ".";
	$buf[] = "..";
	$buf[] = "foobie";
	$buf[] = "baz";

	return -FUSE_ENOERR;
}

?>
