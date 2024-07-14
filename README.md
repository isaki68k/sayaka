Misskey クライアント sayaka ちゃん version 3.7.6 (2024/07/14)
======

ターミナルに特化した Misskey ストリームクライアントです。

* ローカルタイムラインの垂れ流しができます。
* mlterm などの SIXEL 対応ターミナルなら画像も表示できます。
* X68030/25MHz、メモリ12MB でも快適(?)動作。

変更点
---
* 3.7.6 (2024/07/14) … --nsfw オプションを追加。--show-nsfw オプションは廃止。
	FreeBSD でのビルドエラーを修正。
* 3.7.5 (2024/03/12) … Misskey ストリームに --ciphers オプションが効いて
	いなかったのを修正。画像ダウンロードにも --ciphers を適用するよう変更。
* 3.7.4 (2024/03/03) … Linux、OpenBSD でのビルドエラーを修正。
* 3.7.3 (2024/02/23) … Misskey ストリームの死活応答を返せてなかったので対応。
	Blurhash を高速化。
* 3.7.2 (2023/10/19) … WebP 透過画像が表示できていなかったのを修正。
	--show-cw, --show-nsfw オプションを追加。
	NSFW 画像の表示に対応。
	本文をある程度整形。
	外部インスタンス名を表示。
	接続エラーが起きた時の再接続を実装。
* 3.7.1 (2023/10/09) … WebP 画像が表示できないことがあったのを修正。
	emoji 通知メッセージで無限ループになっていたのを修正。
* 3.7.0 (2023/10/09) … Misskey 対応開始。
	Twitter への接続機能は廃止。
	これに伴い、オプション --filter, --home, --no-rest, --post, --token を廃止。
	オプション --twitter, --misskey, --local を追加。
	オプション --ngword-*, --show-ng は一旦廃止。
	オプション --black/--white を --dark/--light に変更。



必要なもの
---
以下は configure のオプションに関わらず必要です。
* C++17 compiler
* BSD make (not GNU make)
* pkg-config
	… pkgsrc なら devel/pkgconf、Ubuntu なら pkg-config です。
* wslay
	… pkgsrc なら www/wslay、
	Ubuntu なら libwslay1 libwslay-dev です。
* libwebp
	… pkgsrc なら graphics/libwebp、
	Ubuntu なら libwebp-dev です。

以下は configure のオプションによって変わります。
* mbedtls 2.x (2.9.0 or later?)
	… デフォルトでは必要です。
	pkgsrc なら security/mbedtls、OpenBSD なら security/polarssl です。
	後述の `--without-mbedtls` を指定した場合は不要です。
* OpenSSL
	… デフォルトでは不要ですが、
	後述の `--without-mbedtls` を指定した場合は必要になります。
	Ubuntu なら libssl-dev です。
* giflib (maybe >= 5.0)、jpeg(libjpeg)、libpng
	… デフォルトでは不要ですが、
	後述の `--without-stb-image` を指定した場合は3つとも必要になります。
	pkgsrc なら graphics/giflib, graphics/jpeg, graphics/png です。


インストール方法
---
ビルドは以下のようにします。

```
% ./configure [<options...>]
% make -DRELEASE sayaka
```

* `--without-stb-image` … デフォルトでは画像ローダには同梱の stb_image
	ヘッダライブラリを使用しています。
	何らかの理由でこれを使用せず外部ライブラリを使用したい場合に
	指定してください。
	その場合は giflib、libjpeg、libpng が必要です。
* `--without-mbedtls` … デフォルトでは SSL ライブラリに mbedTLS を使用します。
	何らかの理由でこれを使用せず OpenSSL を使用したい場合に指定してください。
* `--enable-twitter` … ver 3.6.x 以前に `--record`
	オプションで保存した Twitter のデータの再生が出来ます。
	Twitter に接続できるわけではないので、通常有効にする必要はありません。

make install はないので、出来上がった src/sayaka (実行ファイル) をパスの通ったところにインストールするとかしてください。
ちなみに `make -DRELEASE all` すると、画像ファイルを SIXEL に変換して表示する
sixelv というビューアも出来ます (sayaka の実行には不要です)。


使い方
---
sayaka ver 3.7 以降は Misskey にのみ対応しています。
現在のところローカルタイムラインのみサポートしており、
これはアカウントを持ってなくても閲覧することが出来ます
(アカウントを持たずにサーバのトップページに行くと見えるあれです)。

```
% sayaka --local <servername>
```

なお初回起動時に `~/.sayaka/cache` のディレクトリを作成します。


実装状況
---
* ローカルタイムラインのみサポートしています。
* MFM (Markup language For Misskey) の多くは対応予定はありません。
	* メンションは概ね対応しています。
	* ハッシュタグは概ね対応していますが、まだ一部認識出来ないものがあります。
	* URL は概ね対応しています。`[link](URL)` 形式は URL 部のみ反応します。
	* `<plain>〜</plain>` タグは対応しています。
	* `:emoji:` は未対応です。対応できるかは不明。
	* 太字、イタリック、引用(`>`)、コードブロック、`<center>`、`<small>`
	には反応しません。そのまま表示します。
	* 関数(`$[..]` など) はタグを取り除いて内容のみ表示します。
* リアクションは合計数のみ表示しています。
* 同じ投稿が連続した場合の圧縮表示は未対応です。
* 投稿は出来ません。


主なコマンドライン引数
---
* `--ciphers <ciphers>` 通信に使用する暗号化スイートを指定します。
	今のところ指定できるのは "RSA" (大文字) のみです。
	2桁MHz級の遅マシンでコネクションがタイムアウトするようなら指定してみてください。
	このオプションはメインストリームと画像のダウンロード両方に適用されます。

* `--color <n>` … 色数を指定します。デフォルトは 256色です。
	他はたぶん 16 と 2 (と 8?) くらいを想定しています。

* `--dark` … ダークテーマ (背景色が暗い環境) 用に、
	可能なら明るめの文字色セットを使用します。
	デフォルトでは背景色を自動判別しようとしますが、
	ターミナルが対応していなかったりすると `--light` が選択されます。

* `--eucjp` … 文字コードを EUC-JP に変換して出力します。
	VT382J 等の EUC-JP (DEC漢字) に対応したターミナルで使えます。

* `--font <W>x<H>` … フォントの幅と高さを `--font 7x14` のように指定します。
	デフォルトではターミナルに問い合わせて取得しますが、
	ターミナルが対応してない場合などは勝手に 7x14 としますので、
	もし違う場合はこの `--font` オプションを使って指定してください。
	アイコンと画像はここで指定したフォントサイズに連動した大きさで表示されます。

* `--force-sixel` … SIXEL 画像出力を(強制的に)オンにします。
	このオプションを指定しなくても、ターミナルが SIXEL 対応であることが
	判別できれば自動的に画像出力は有効になります。
	ターミナルが SIXEL を扱えるにも関わらずそれが判別できなかった場合などに
	指定してください。

* `--full-url` … URL が省略形になる場合でも元の URL を表示します。
	Twitter 専用です。

* `--jis` … 文字コードを JIS に変換して出力します。
	NetBSD/x68k コンソール等の JIS に対応したターミナルで使えます。

* `--light` … ライトテーマ (背景色が明るい環境) 用に、
	可能なら濃いめの文字色セットを使用します。
	デフォルトでは背景色を自動判別しようとしますが、
	ターミナルが対応していなかったりすると自動で選択されます。

* `--local <servername>` … 指定のサーバの Misskey
	ローカルタイムラインを表示します。
	アカウントを持ってなくても表示できます。

* `--mathalpha` … Unicode の [Mathematical Alphanumeric Symbols](https://en.wikipedia.org/wiki/Mathematical_Alphanumeric_Symbols)
	を全角英数字に変換します。
	お使いのフォントが Mathematical Alphanumeric Symbols に対応しておらず
	全角英数字なら表示できる人を救済するためです。

* `--max-image-cols <n>` … 1行に表示する画像の最大数です。
	デフォルトは 0 で、この場合ターミナル幅を超えない限り横に並べて表示します。
	ターミナル幅、フォント幅が取得できないときは 1 として動作します。

* `--misskey` … Misskey モードにします。
	現状デフォルトなので、通常指定する必要はありません。

* `--no-color` … テキストをすべて(色を含む)属性なしで出力します。
	`--color` オプションの結果が致命的に残念だった場合の救済用です。

* `--no-image` … SIXEL 画像出力を強制的にオフにします。
	このオプションを指定しなくても、ターミナルが SIXEL 非対応であることが
	判別できれば自動的に画像出力はオフになります。

* `--no-combine` … `U+20DD`〜`U+20E4` の囲み文字の前にスペースを追加します。
	本来これらは直前の文字を囲み合成する Unicode 文字ですが、
	Twitter では絵文字セレクタとともに利用して安易に文字や数字を強調する
	手段として使われがちです。
	そのため、端末およびフォントによって合成した結果読めないものが
	表示されてしまう環境では、強調された肝心な部分だけが読めないという
	残念なことが起こります。それが場合によってはマシになるかも知れません。

	例えば「半角数字の1をキートップの絵文字で囲んだ文字」は

	| option | display |
	| --- | --- |
	| `--no-combine` なし(デフォルト) | `1⃣`|
	| `--no-combine` あり | `1 ⃣` |

	のように表示されると思います。
	(ブラウザでどう見えるかとご使用の端末でどう見えるかは別なので、
	このファイルを `grep no-combine ./README.md` などで表示してみてください)

* `--nsfw [ show | blur | no ]` … Misskey の NSFW (Not Safe For Work、閲覧注意)
	画像の表示方法を指定します。
	`show` なら元画像を表示、`blur` ならぼかし表示します。
	`no` なら表示しません。
	デフォルトは `blur` です。

* `--play` … ユーザストリームの代わりに標準入力の内容を再生します。

* `--progress` … 接続完了までの処理を表示します。
	遅マシン向けでしたが、
	フィルタストリーム廃止後の現在では、キャッシュ削除しかすることがないので
	あまり意味がないかも知れません。

* `--protect` … 鍵付きアカウントのツイートを表示しません。
	Twitter 専用です。

* `--record <file>` / `--record-all <file>` …
	ストリームで受信した JSON のうち `--record-all` ならすべてを、
	`--record` なら概ね表示するもののみを `<file>` に記録します。
	いずれも `--play` コマンドで再生できます。

* `--show-cw` … Misskey の CW (Contents Warning、内容を隠す) 付き投稿であっても
	本文を表示します。

* `--timeout-image <msec>` … 画像取得のサーバへの接続タイムアウトを
	ミリ秒単位で設定します。
	0 を指定すると connect(2) のタイムアウト時間になります。
	デフォルトは 3000 (3秒)です。

* `--twitter` … Twitter モードにします。
	`--play` オプションで Twitter 時代のデータを再生する場合に指定します。
	Twitter に接続できるわけではありません。

* `--x68k` … NetBSD/x68k (SIXEL パッチ適用コンソール)
	のためのプリセットオプションで、
	実際には `--color x68k --font 8x16 --jis --dark --progress --ormode on --palette off` と等価です。

その他のコマンドライン引数
---
* `-4`/`-6` … IPv4/IPv6 のみを使用します。
	このオプションは今の所画像のダウンロードのみに適用されます。

* `--eaw-a <n>` … Unicode の East Asian Width が Ambiguous な文字の
	文字幅を 1 か 2 で指定します。デフォルトは 1 です。
	というか通常 1 のはずです。
	ターミナルとフォントも幅が揃ってないとたぶん悲しい目にあいます。

* `--eaw-n <n>` … Unicode の East Asian Width が Neutral な文字の
	文字幅を 1 か 2 で指定します。デフォルトは 2 です。
	ターミナルとフォントも幅が揃ってないとたぶん悲しい目にあいます。

* `--max-cont <n>` … ~~同一ツイートに対するリツイートが連続した場合に
	表示を簡略化しますが、その上限数を指定します。デフォルトは 10 です。
	0 以下を指定すると簡略化を行いません(従来どおり)。~~
	未復旧です。

* `--ormode <on|off>` … on なら SIXEL を独自実装の OR モードで出力します。
	デフォルトは off です。
	ターミナル側も OR モードに対応している必要があります。

* `--palette <on|off>` … on なら SIXEL 画像にパレット定義情報を出力します。
	デフォルトは on です。
	NetBSD/x68k SIXEL 対応パッチのあててある俺様カーネルでは、
	SIXEL 画像内のパレット定義を参照しないため、off にすると少しだけ
	高速になります。
	それ以外の環境では on のまま使用してください。


おまけ(sixelv)
---
`sixelv` という SIXEL ビューワを同梱しています。
単に sayaka ちゃん開発中に SIXEL デコーダ部分だけを独立させたテストプログラムだったのですが、
mlterm などの SIXEL 対応端末ではその場で画像が表示できて結構便利です。
```
% sixelv -w 256 foo.jpg
```
みたいに使います。詳細は `--help` オプションを見てください。
サポートしている画像形式は次の通りです (sayaka ちゃんも同様です)。
* JPEG、PNG、Blurhash
* GIF、WebP (アニメーションはいずれも1枚目のみ)
* BMP、PNM、TGA(?)、PSD、HDR、Softimage PIC(?) …
	stb_image デコーダを使っている場合のみ対応します。


ライセンス
---
* sayaka 自体は 2-clause BSD ライセンスです。
* 同梱のヘッダライブラリ nlohmann/*.hpp は MIT ライセンスです。
* 同梱のヘッダライブラリ stb/stb_image.h は public domain として利用しています。


更新履歴
---
* 2023/07/19 … Twitter に接続できなくなりました。
* 3.6.5 (2023/07/09) … --protect オプションを復活。
* 3.6.4 (2023/06/11) … 画像ローダをデフォルトで libjpeg, libpng, giflib
	から stb_image に変更。SSL/TLS ライブラリに OpenSSL もサポート。
	OpenBSD/amd64、Ubuntu 22.04 でのビルド修正。
* 3.6.3 (2023/03/26) … ビルドエラーを修正。
* 3.6.2 (2023/03/26) … extended_tweet 対応を復旧。
	--no-combine オプションを実装。
* 3.6.1 (2023/03/21) … ゼロ除算を修正。接続間隔を調整。
* 3.6.0 (2023/03/18) … フィルタストリーム廃止に伴い REST API (v1.1) で仮復旧。
	--reconnect オプションを廃止。
* 3.5.5 (2023/02/28) … GIF 画像をサポート。
* 3.5.4 (2022/11/08) … --force-sixel オプションを実装。
	--progress 指定時の表示エンバグ修正。
* 3.5.3 (2022/02/15) … キーワードの複数指定に対応。
	--mathalpha オプションを実装。
* 3.5.2 (2021/07/22) … 自動再接続と --reconnect オプションを実装。
* 3.5.1 (2021/03/18) … アイコン取得を HTTPS でなく HTTP, HTTPS
	の順で試すよう変更。
	エラー処理をいくつか改善。
* 3.5.0 (2021/03/03) … C++ に移行し vala 版廃止。
	画像は現在のところ JPEG, PNG のみ対応。
	ターミナル背景色の自動取得を実装。
	--protect、--support-evs オプション廃止。
	--noimg オプションを廃止 (--no-image に変更)。
	userstream 時代の録画データの再生機能廃止。
* 3.4.6 (2020/11/10) … --no-image 指定時のアイコン代わりのマークが
	化ける場合があったのでマークを変更。
* 3.4.5 (2020/05/15) … 表示判定を再実装して
	フォローから非フォローへのリプライが表示されてしまう場合があるのを修正。
	NG ワード判定が漏れるケースがあったのを修正。
	`--record-all` オプションを実装。
* 3.4.4 (2020/05/01) … Linux で SIGWINCH 受信で終了していたのを修正。
	リツイートの連続表示を修正。SIXEL 判定のタイムアウトを延長。
	--token オプションの動作を変更。ログ周りを色々修正。
* 3.4.3 (2020/02/15) … 引用ツイートが表示されないケースがあったのを修正。
	SIXEL 対応ターミナルの判別を改善。
* 3.4.2 (2020/02/01) … 2色のターミナルに対応。--no-color オプションを実装。
	--no-image オプションを用意 (従来の --noimg も使用可)。
	SIXEL 非対応ターミナルならアイコンの代わりにマークを表示。
* 3.4.1 (2020/01/12) … 疑似ホームタイムラインの調整。
	SIXEL 非対応ターミナルを自動判別してみる。
* 3.4.0 (2020/01/05) … フィルタストリームによる擬似ホームタイムラインに対応。
* 3.3.3 (2020/01/04) … Linux でのビルドエラーを修正。
* 3.3.2 (2018/01/02) … pkgsrc-2017Q4 (vala-0.38.1以上) でのビルドに対応。
	画像は Content-Type が image/* の時のみ表示するようにしてみる。
	mbedTLS-2.4.2 に更新。
	--timeout-image オプションを実装。
* 3.3.1 (2016/12/23) … リソースリークを含むバグ修正。
* 3.3.0 (2016/11/25) … libcurl ではなく mbedTLS に移行してみる。
	--full-url オプション、--progress オプションを実装。
	--sixel-cmd オプション廃止、PHP 版サポート廃止。
	EUC-JP/JIS に変換できない文字の処理を追加。
	画像の高品質化、高速化いろいろ。
* 3.2.2 (2016/09/25) … glib-networking ではなく libcurl に移行してみる。
	--post オプション、--ciphers オプションを実装。
	extended_tweet の表示に対応。
* 3.2.1 (2016/04/24) … --filter オプション、--record オプションを実装。
	NGワード編集機能実装。
	「リツイートを非表示」にしたユーザに対応。
	shindanmaker の画像サムネイルに対応。
	Unicode 外字をコード表示。
	連続するリツイート・ふぁぼを圧縮して表示。
* 3.2.0 (2016/02/24) … vala 版サポート。
	画像の横方向への展開サポート (vala 版のみ)。
	SQLite3 データベース廃止 (PHP 版のみ)。
* 3.1.0 (2015/07/26) …
	--font オプションの仕様変更。
	VT382(?)など(いわゆる)半角フォントの縦横比が 1:2 でない環境に対応。
	--noimg の時に改行が一つ多かったのを修正。
* 3.0.9 (2015/06/14) …
	--eucjp、--protect オプションを追加しました。
	またコメント付き RT の仕様変更(?)に追従しました。
* 3.0.8 (2015/05/03) …
	--font オプションを追加して、画像サイズを連動するようにしました。
* 3.0.7 (2015/04/19) … コメント付き RT の表示に対応してみました。
* 3.0.6 (2014/12/06) … libsixel 1.3 に対応。libsixel 1.3 未満は使えません。
* 3.0.5 (2014/10/23) … 本文を折り返して表示。


。
---
[@isaki68k](https://twitter.com/isaki68k/)  
[差入れ](https://www.amazon.co.jp/hz/wishlist/ls/3TXVBRKSKTF31)してもらえると喜びます。
