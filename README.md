twitter クライアント sayaka ちゃん version 3.0.4 (2014/10/18)
======

ターミナルに特化した twitter クライアントです。

* ユーザストリームの垂れ流しが出来ます。
* mlterm などの sixel 対応ターミナル用です。
* PHP スクリプトです。
* ruby じゃないので遅マシンでも快適 (たぶん)

最近の更新
---
* 3.0.4 … --jis オプションを追加しました。
* 3.0.3 … インストール手順を更新、起動スクリプトを用意しました。


必要なもの
---
* PHP
  - version 5.3 以上
  - CLI 版が必要です。シェルから `php -v` でバージョンとか出れば OK です。
  - curl, json, pdo_sqlite モジュールが必要です。
  - x68k で `--jis` オプションを使うには mbstring モジュールも必要です。
  - X window で SIGWINCH によるターミナルサイズ変更に追従したい場合は pcntl モジュールが必要です。コンソールなどターミナルサイズが変わらないところで使うなら不要です。

* pkgsrc なら
lang/php, www/php-curl, textproc/php-json,
databases/php-pdo、databases/php-pdo_sqlite
をインストールして /usr/pkg/etc/php.ini に以下の行を追加します。
converters/php-mbstring、devel/php-pcntl は必要なら追加してください。
```
extension=curl.so
extension=json.so
extension=pdo.so
extension=pdo_sqlite.so
(extension=mbstring.so)
(extension=pcntl.so)
```

* PHP を野良ビルドする場合以下のオプションくらいで行けそうです。
--with-curl 等に指定するパスは要不要も含めて環境に合わせて適宜。
また括弧書きしたオプションについては不要な環境と必要な環境があるようです。
--enable-mbstring は x68k で使いたい人だけ指定すればよいですが、
これだけでバイナリサイズが 1.5MB 増えます。
同様に --enable-pcntl はターミナルサイズの変更に対応したい場合だけ指定すればよいです。
```
% ./configure
   --disable-all
   --disable-cgi
   --enable-json
   --enable-filter
   --enable-pdo
   --with-sqlite3(=/usr/pkg)
   --with-curl(=/usr/pkg)
  (--enable-hash)
  (--enable-libxml)
  (--enable-mbstring)
  (--enable-pcntl)
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
なくても動作します。
というか将来廃止予定。


インストール方法
---
展開したディレクトリ内だけで動作しますので、
適当なところに展開してください。
といいつつ後述の理由により ~/.sayaka/ に展開することを前提にしています。

次に ~/.sayaka/ の sayaka.sh をパスの通ったところ
(`/usr/local/bin` やあるいは `$HOME/bin` など)
に sayaka にリネームして置くか、あるいはシンボリックリンクをはります。
```
(例)
% cd ~/bin
% ln -s ~/.sayaka/sayaka.sh sayaka
```

~/.sayaka 以外のところに展開した場合は sayaka.sh の先頭のほうにある
`SAYAKA_HOME` 変数のパスを展開先ディレクトリに書き換えてから使ってください。
(以下 ~/.sayaka を適宜読み替えてください)


とりあえず使ってみる
---
まず初期化を行います。これは ~/.sayaka に作業ディレクトリを作成します。
```
% sayaka init
```

クライアントの認証を行います。
URL が表示されるのでブラウザで接続してください。
表示された PIN コードを入力します。
```
% sayaka authorize
Authroize URL is: https://twitter.com/...

Input PIN code:
```

これでユーザストリームが表示できるようになります。
```
% sayaka stream
```


コマンドライン引数など
---
以下のうちいずれかで動作モードを指定します。
* `--stream` … ユーザストリームモードです。
	`stream` あるいは `s` と省略することも出来ます。
* `--post <msg>` … 引数 <msg> をツイートします。
* `--pipe`
	標準出力の内容をツイートします。
* `--play <file>`
	ユーザストリームの代わりに `<file>` の内容を再生します。
	`<file>` が `-` ならファイルの代わりに標準入力から読み込みます。

以下のオプションは `--stream` の時に指定できます。
* `--color <n>` … 色数を指定します。デフォルトは 256色です。
	他はたぶん 16 と 2 くらいを想定しています。

* `--white` … 白背景用の色合いに変更します。

* `--noimg` … SIXEL 画像を一切出力しません。SIXEL 非対応ターミナル用。

* `--jis` … 文字コードを JIS に変換して出力します。
	JIS を受け取るターミナル用とか。

* `--record <file>` … ユーザストリームで受信したすべてのデータを
	`<file>` に記録します。`--play` コマンドで再生できます。

なお、PHP -S サーバモードでも起動します。
```
% cd ~/.sayaka
% php -S localhost:8000
% curl localhost:8000/sayaka.php
```


TODO
---
* 遅マシン展示デモ用の軽量化
* 認証も TwistOAuth.php に一本化したい (置き換え方が分からなかった)

