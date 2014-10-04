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
  - version 5.2 以上くらい?
  - CLI 版が必要です。シェルから `php -v` でバージョンとか出れば OK です。

* PHP には以下のモジュールが必要です。
  - curl
  - json
  - pdo
  - pdo_sqlite

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


インストール方法
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


TODO
---
* 白背景用のカラー調整?
* img2sixel なしオプション? --noimg ?
* NGワード実装
* 遅マシン展示デモ用の軽量化
* 認証も TwistOAuth.php に一本化したい (置き換え方が分からなかった)

