<?php
/*
 * sayaka - twitter client
 */

/*
 * Copyright (C) 2011-2014 Tetsuya Isaki
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

	// コンシューマキーとシークレット
	$consumer_key = "jPY9PU5lvwb6s9mqx3KjRA";
	$consumer_secret = "faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw";

	// どうするかね
	$basedir    = "{$_SERVER['HOME']}/.sayaka/";
	$cachedir   = "{$basedir}/cache";
	$tokenfile  = "{$basedir}/token.json";
	$ngwordfile = "{$basedir}/ngword.json";
	$debugfile  = "{$basedir}/log.txt";

	// タイムゾーン XXX どうするかね
	$tz = "Asia/Tokyo";

	define("DEBUG", 0);
	define("DEBUGFILE", $debugfile);


// タイムゾーン設定
function setTimeZone()
{
	global $tz;

	// タイムゾーンを取得
	if (!date_default_timezone_set($tz)) {
		print "setTimeZone failed\n";
	}
}

// twitter 書式の日付時刻を UnixTime (整数) にして返す。
// 入力形式は複数あるという噂もあるがとりあえず確認出来てるのは以下：
//	Wed Nov 18 18:54:12 +0000 2009
function conv_twtime_to_unixtime($instr)
{
	$month = array(
		"Jan" =>  1, "Feb" =>  2, "Mar" =>  3, "Apr" =>  4,
		"May" =>  5, "Jun" =>  6, "Jul" =>  7, "Aug" =>  8,
		"Sep" =>  9, "Oct" => 10, "Nov" => 11, "Dec" => 12,
	);
	list ($wday, $monname, $mday, $t, $tz, $year) = preg_split("/ /", $instr);
	list ($hour, $min, $sec) = preg_split("/:/", $t);

	$mday = intval($mday);
	$year = intval($year);
	$hour = intval($hour);
	$min  = intval($min);
	$sec  = intval($sec);

	// mktime() でタイムゾーン考慮できないので GMT のまま変換
	$unixtime = gmmktime($hour, $min, $sec, $month[$monname], $mday, $year);

	// タイムゾーン補正
	if ($tz != "+0000") {
		// XXX 未対応だが観測してる限り +0000 のみっぽいので問題ない?
	}

	return $unixtime;
}

?>
