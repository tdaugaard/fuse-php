<?php

if( !extension_loaded("fuse") )
	dl( "fuse." . PHP_SHLIB_SUFFIX );

class MyFuseWrapper extends FuseWrapper
{
	public function FsGetAttr( $path, $stbuf )
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

	public function FsOpen( $path, $fi )
	{
		if( ($fi->flags & FUSE_O_ACCMODE) != FUSE_O_RDONLY )
			return -FUSE_ENOACCES;

		return -FUSE_ENOERR;
	}

	public function FsRead( $path, &$buf, $size, $offset, $fi )
	{
		$data = "You've implemented your first FUSE file system with PHP!";

		if( $offset > strlen($data) )
			return 0;

		$buf = substr( $data, $offset, $size );

		return strlen($data);
	}

	public function FsReadDir( $path, &$buf )
	{
		if( strcmp($path, "/") != 0 )
			return -FUSE_ENOENT;

		$buf[] = ".";
		$buf[] = "..";
		$buf[] = "helloworld.txt";

		return -FUSE_ENOERR;
	}
}

$wrapper = new MyFuseWrapper();
$wrapper->Init( $argv );

?>
