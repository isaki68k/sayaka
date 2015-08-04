#!/usr/pkg/bin/php
<?php
//
// pkgsrc でインストールした php の php.ini をいろいろ更新する
//
// Copyright (C) 2014 isaki@NetBSD.org
//

	date_default_timezone_set("Asia/Tokyo");

	switch ($_SERVER['argv'][1]) {
	 case "-notice":
		cmd_notice($filename);
		break;
	 case "ext":
		cmd_ext($filename);
		break;
	 default:
		print "usage: {$_SERVER['argv'][0]} [ext | -notice]\n";
		break;
	}
	exit(0);
?>
<?php
//
// extension= 行を現状に揃えて更新する
//
function cmd_ext($filename)
{
	// php -i から情報取得
	$php_info = `php -i`;
	if (preg_match("/Loaded Configuration File => ([^\s]+)/", $php_info, $m)) {
		$php_ini_filename = $m[1];
	}
	if (preg_match("/extension_dir => ([^\s]+)/", $php_info, $m)) {
		$extension_dir = $m[1];
	}

	// pkgsrc なのでそうじゃない php が起動してたらエラー
	if ($php_ini_filename != "/usr/pkg/etc/php.ini") {
		print "This is not pkgsrc.\n";
		exit(1);
	}

	// 実際にインストールされてるモジュールを列挙する
	$extensions = array();
	$dir = opendir($extension_dir);
	while (($name = readdir($dir))) {
		if (preg_match("/\.so$/", $name)) {
			$extensions[] = "extension = {$name}\n";
		}
	}

	$file = file($php_ini_filename);
	$newfile = array();

	// 先頭から
	for ($i = 0; ($buf = $file[$i]) != null; $i++) {
		if (preg_match("/^extension\s*=/", $buf)) {
			continue;
		}
		$newfile[] = $buf;

		// Dynamic Extensions (の次の行) まで
		if (preg_match("/^; Dynamic Extensions ;/", $buf)) {
			$i++;
			break;
		}
	}
	$newfile[] = $file[$i++];
	$newfile[] = "\n";

	// ここに extension= を追加
	foreach ($extensions as $e) {
		$newfile[] = $e;
	}
	$newfile[] = "\n";

	// 続く空行を除く
	for (; ($buf = $file[$i]) != null; $i++) {
		if (preg_match("/^extension\s*=/", $buf)) {
			continue;
		}
		if (preg_match("/^\s*$/", $buf)) {
			continue;
		}
		break;
	}

	// 残り
	for (; ($buf = $file[$i]) != null; $i++) {
		if (preg_match("/^extension\s*=/", $buf)) {
			continue;
		}
		$newfile[] = $buf;
	}

	// 再結合
	$new_php_ini = join($newfile, "");

	// ファイル更新
	print "rename {$php_ini_filename} -> {$php_ini_filename}.bak\n";
	rename($php_ini_filename, "{$php_ini_filename}.bak");
	print "update {$php_ini_filename}\n";
	file_put_contents($php_ini_filename, $new_php_ini);
	return 0;
}

//
// error_reporting に E_NOTICE を追加する
//
function cmd_notice()
{
	print "Not implemented yet\n";
}
?>
