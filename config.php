<?php
/*
 * sayaka - twitter client
 */
/*
 * Copyright (C) 2011-2014 Tetsuya Isaki
 * Copyright (C) 2011 Y.Sugahara (moveccr)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

	require_once "subr.php";

	// XXX TwistOAUth に一本化したい
	require_once "twitteroauth.php";

	switch ($_SERVER['argv'][1]) {
	 case "init":
		init_files();
		upgrade_files();
		break;
	 case "upgrade":
		upgrade_files();
		break;
	 case "authorize":
		authorize();
		break;
	 default:
		usage();
		break;
	}
	exit(0);
?>
<?php
function usage()
{
	printf("usage: %s <cmd>\n", basename($_SERVER['argv'][0]));
	print "\tinit\n";
	print "\tupgrade\n";
	print "\tauthorize\n";
}

// 初期化
// XXX パーミッションどうするのがいいかね
function init_files()
{
	global $datadir;
	global $cachedir;
	global $datadb;
	global $configdb;

	print "Initializing...\n";

	// ディレクトリを作成
	chdir(dirname(__FILE__));
	if (!file_exists($datadir)) {
		mkdir($datadir, 0777, true);
		chmod($datadir, 0777 & ~umask() | 07);
	} else {
		print "data directory \"{$datadir}\" already exists\n";
	}
	if (!file_exists($cachedir)) {
		mkdir($cachedir, 0777, true);
		chmod($cachedir, 0777 & ~umask() | 07);
	} else {
		print "cache directory \"{$cachedir}\" already exists\n";
	}

	// データベースを作成
	if (!file_exists($configdb)) {
		$db = new sayakaSQLite3($configdb);
		$db->close();
		chmod($configdb, 0606);
	} else {
		print "config db \"{$configdb}\" already exists\n";
	}
	if (!file_exists($datadb)) {
		$db = new sayakaSQLite3($datadb);
		$db->close();
		chmod($datadb, 0606);
	} else {
		print "data db \"{$datadb}\" already exists\n";
	}

	print "done.\n";
}

// データベースのアップグレード
function upgrade_files()
{
	global $configdb;
	global $datadb;
	global $tz;

	// バージョンを取得
	$ver = get_config_version();
	print "current version is {$ver}\n";

	if ($ver > 0) {
		backup_datafile($configdb);
		backup_datafile($datadb);
	}

	$db = new sayakaSQLite3($configdb);
	$db->begin();

	try {
		switch ($ver) {
		 case 0:
			// ver 0->1: 作成直後の状態
			print "0->";
			$q = <<<__EOM__
create table t_config (
	id integer primary key,
	version integer,
	timezone text
);
create table t_token (
	id integer primary key,
	token text,
	secret text,
	request_token text,
	request_secret text
);
create table t_mute (
	id text primary key
);
create table t_ngword (
	id integer primary key,
	ngword text,
	user_id unsigned big int
);
__EOM__;
			$db->exec("{$q}");
			$db->exec("insert into t_config values (1, 0, '{$tz}')");
			setversion($db, 1);
			// FALLTHROUGH

		 default:
			// 最新バージョン
			print "1\n";
		}
	} catch (Exception $e) {
		print "Exception: {$e}\n";
	}

	$db->commit();
	$db->close();
	print "Upgraded.\n";
}

// データベースファイルをバックアップ
function backup_datafile($filename)
{
	$backupfile = sprintf("%s.bak.%s.sq3",
		$filename,
		@strftime("%Y%m%d%H%M%S")
	);

	copy($filename, $backupfile);
	print "backup: {$backupfile}\n";
}

// データベースのバージョンを取得
// 0 なら初期状態
function get_config_version($db = null)
{
	global $configdb;

	$opendb = false;
	if (is_null($db)) {
		$db = new sayakaSQLite3($configdb);
		$opendb = true;
	}

	// バージョンを取得
	$ver = 0;
	$r = @$db->querySingle("select version from t_config where id=1");
	if ($r !== false) {
		$ver = $r;
	}

	if ($opendb) {
		$db->close();
	}

	return $ver;
}

// config.db にバージョンをセットする
function setversion($db, $ver)
{
	// 内部用関数なので信用する
	$db->exec("update t_config set version={$ver} where id=1");
}

// twitter 認証 (コマンドライン)
function authorize()
{
	global $consumer_key;
	global $consumer_secret;
	global $configdb;

	$tw = new TwitterOAuth($consumer_key, $consumer_secret);

	// リクエストトークンの取得
	$request_token = $tw->getRequestToken();
	// 認証用URLの取得
	$url = $tw->getAuthorizeURL($request_token);
	$token = $request_token["oauth_token"];
	$secret = $request_token["oauth_token_secret"];
	unset($tw);

	print "Authroize URL is: {$url}\n";
	print "\n";
	print "Input PIN code:";
	fflush(STDOUT);

	$pin = fgets(STDIN);
	$pin = rtrim($pin);

	// 認証番号からアクセストークンを取得する
	$tw = new TwitterOAuth($consumer_key, $consumer_secret,
		$token, $secret);
	$token = $tw->getAccessToken($pin);
	$oauth_token = $token["oauth_token"];
	$oauth_token_secret = $token["oauth_token_secret"];

	if (empty($oauth_token) || empty($oauth_token_secret)) {
		print "Authorization failed.\n";
		return;
	}

	$db = new sayakaSQLite3($configdb);
	$db->query("replace into t_token (id, token, secret)"
		. " values (1, '{$oauth_token}', '{$oauth_token_secret}')");
	print "Authorization succeeded.\n";
}
?>
