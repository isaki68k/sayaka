twitter クライアント sayaka ちゃん
======

ターミナルに特化した twitter クライアントです。

* ユーザストリームの垂れ流しが出来ます。
* mlterm などの sixel 対応ターミナル用です。
* PHP スクリプトです。
* ruby じゃないので遅マシンでも快適 (たぶん)


必要なもの
---
* PHP
  - version 5.3 以上
  - CLI 版が必要です。シェルから `php -v` でバージョンとか出れば OK です。
  - curl, json, pdo_sqlite モジュールが必要です。

* pkgsrc なら
lang/php, www/php-curl, textproc/php-json,
databases/php-pdo、databases/php-pdo_sqlite をインストールして、
/usr/pkg/etc/php.ini に以下の行を追加します。
```
extension=curl.so
extension=json.so
extension=pdo.so
extension=pdo_sqlite.so
```

* PHP を野良ビルドする場合以下のオプションくらいで行けそうです。
```
% ./configure
   --disable-all
   --disable-cgi
   --enable-json
   --enable-filter
   --enable-pdo
   --with-sqlite3=/usr/pkg
   --with-curl=/usr/pkg
% make
```
NetBSD-6.1.4/amd64 + pkgsrc-2014Q2 の場合、
pkgsrc 版のバイナリが本体 7.6MB + モジュール 0.3MB くらい? ですが、
このオプションで作った野良ビルド版では本体 3.7MB 程度になります。
メモリの少ないマシンではこの差は効くかも。

* E_NOTICE レベルのワーニングがん無視なので、
/usr/pkg/etc/php.ini の error_reporting に
`& ~E_NOTICE` を追加するなどして
E_NOTICE レベルのレポートを落としておくことをお勧めします。
```
- error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
+ error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT & ~E_NOTICE
```

* curl (必須) …
pkgsrc/www/curl をインストールするなどして、
パスの通ってるところに置いといてください。

* img2sixel (オプション) …
pkgsrc-current/graphics/libsixel をインストールするか、
libsixel を make して img2sixel をパスの通ったところに置いてください。
なければアイコンや写真などが表示できないだけで、動作はします。

* giftopnm (オプション) …
pkgsrc/graphics/netpbm をインストールするなどして、
パスの通ってるところに giftopnm を置いてください。
なくても動作します (img2sixel が GIF 画像も直接扱います) が、
giftopnm 通したほうが表示できる GIF 形式が増えるかも知れません。


とりあえず使ってみる
---
適当なところに展開してください。
展開したディレクトリ内だけで動作します。


初期化を行います。これは、ここに作業ディレクトリを作ります。
```
% php config.php init
```

クライアントの認証を行います。
URL が表示されるのでブラウザで接続してください。
表示された PIN コードを入力します。
```
% php config.php authorize
Authroize URL is: https://twitter.com/...

Input PIN code:
```

これでユーザストリームが表示できるようになります。
```
% php sayaka.php --stream
```


コマンドライン引数など
---
以下のうちいずれかで動作モードを指定します。
* `--stream` … ユーザストリームモードです。
* `--post <msg>` … 引数 <msg> をツイートします。
* `--pipe`
	標準出力の内容をツイートします。
* `--play <file>`
	ユーザストリームの代わりに `<file>` の内容を再生します。
	`<file>` が `-` ならファイルの代わりに標準入力から読み込みます。

以下のオプションは --stream の時に指定できます。
* `--color <n>` … 色数を指定します。デフォルトは 256色?
	他はたぶん 16 と 2 くらいを想定しています。

* `--white` … 白背景用の色合いに変更します。調整中。

* `--noimg` … SIXEL 画像を一切出力しません。SIXEL 非対応ターミナル用。

* `--record <file>` … ユーザストリームで受信したすべてのデータを
	`<file>` に記録します。`--play` コマンドで再生できます。

なお、PHP -S サーバモードでも起動します。
```
% php -S localhost:8000
% curl localhost:8000/sayaka.php
```


TODO
---
* 遅マシン展示デモ用の軽量化
* 認証も TwistOAuth.php に一本化したい (置き換え方が分からなかった)

