SIXEL 画像ビューア sixelv version 3.8.0 (2024/09/04)
===

SIXEL 画像ビューアです。
sayaka ちゃんの画像処理部分を抜き出して独立させたものに、
sayaka ちゃんでは選択できない細かいパラメータの指定もできます。

インストール方法
---
sayaka ちゃんをビルドする際にターゲットを all にすると作られます。
```
% ./configure
% make -DRELEASE all
```

`make install` はないので、出来上がった `src/sixelv` (実行ファイル)
をパスの通ったところにインストールするとかしてください。

サポートしている画像形式
---
* JPEG
* PNG (透過は未対応)
* GIF (アニメーションは1枚目のみ、透過は未対応)
* Webp (アニメーションは1枚目のみ、透過は未対応)
* BMP
* Blurhash
* PNM

使い方
---
```
sixelv [<options>...] [-|<file|url...>]
```

画像は `<file>` か `<url>` (HTTP/HTTPS) で複数指定出来ます。
`-` なら標準入力から1ファイルだけ読み込みます。
オプションは次の通りです。
中には明らかに実験用なものも含まれています。

* `-c,--color=<mode>` … 色モードを指定します。デフォルトは `256` です。
	* `256` … 固定256色モードです。
	* `16` … ANSI 互換の 16色モードです。
	* `8` … RGB 8色モードです。
	* `2` … モノクロです。
	* `gray<N>` … グレー `<N>` 階調です。
		`<N>` は `2` から `256` まで指定できます。
		`gray2` は `2`(モノクロ) と等価です。
	* `gray` … `gray256` です。

* `-w,--width=<width>` … 画像の表示幅(ピクセル)を指定します。
	0 を指定すると原寸(あるいは無指定)を意味します。デフォルトは 0 です。
* `-h,--height=<height>` … 画像の表示高さ(ピクセル)を指定します。
	0 を指定すると原寸(あるいは無指定)を意味します。デフォルトは 0 です。
* `--resize-axis=<axis>` … 画像をリサイズする際の基準軸を指定します。
	デフォルトは `both` です。
	* `both`
	* `width`
	* `height`
	* `long`
	* `short`
	* `scaledown-both`/`sdboth`
	* `scaledown-width`/`sdwidth`
	* `scaledown-height`/`sdheight`
	* `scaledown-long`/`sdlong`
	* `scaledown-short`/`sdshort`
* `-r,--reduction=<method>` … 減色/リサイズ方法を指定します。
	デフォルトは `high` です。
	* `simple`/`none` … 最近傍(Nearest Neighbor) 法です。
	* `high` … 誤差拡散法です。
* `-d,--diffusion=<diffusion` … `-r high` の時の誤差拡散アルゴリズムを指定します。デフォルトは `fs` です。
	* `fs` … Floyd Steinberg
	* `atkinson` … Atkinson
	* `jajuni` … Jarvis, Judice, Ninke
	* `stucki` … Stucki
	* `burkes` … Burkes
	* `2`
	* `3`
	* `rgb`
* `--bn,--blurhash-neighbor` … Blurhash 画像を再近傍法で拡大生成します。
* `--gain=<gain>` … 出力ゲインを調整できます。
	`<gain>` は `0.0` から `2.0` まで実数で指定でき、デフォルトは `1.0` です。
* `-O,--output-format=<fmt>` … 出力画像形式を指定します。
	デフォルトは `sixel` です。
	* `bmp` … (SIXEL 出力用にパレット化した状態の画像を) BMP 形式で出力します。
	* `sixel` … SIXEL です。
* `-o <filename>` … 出力ファイル名を指定します。
	`-` は標準出力を表します。デフォルトは `-` です。
	入力画像が複数ある時は指定できません。
* `--ormode` … SIXEL を OR モードで出力します。
	ターミナル側が OR モードに対応している必要がありますが、
	検出方法がありません。(mlterm は対応しています)
* `--suppress-palette` … SIXEL 文字列のうちパレット定義部分の出力を抑制します。
	端末が RGB 8色や ANSI 16色など固定で任意パレットを扱えない場合は
	そもそもパレット定義を送出する必要がなく、
	受け取ったターミナル側もそれを読み飛ばす処理が不要になるため、
	理論上は処理が軽くなることが期待されますが、通常は誤差レベルです。
* `-i,--ignore-error` … 画像が複数枚指定された場合に
	エラーが起きても次のファイルの処理に移ります。
* `--ciphers=<ciphers>`
* `--ipv4` / `--ipv6`

Blurhash について
---
Blurhash 画像は自身で大きさもアスペクト比の情報を持っていません。
このため、デフォルトでは内部状態を縦横 10倍したものを表示します
(内部の画素数が 6x6 なら 60x60 ピクセル)。
元画像のアスペクト比が 1:1 でない場合は `-w`、`-h` オプションで
希望の表示サイズを指定してください。


ライセンス
---
* sixelv 自体は 2-clause BSD ライセンスです。
* サードパーティライブラリについては [NOTICES.md](NOTICES.md) を参照してください。


更新履歴
---
* 3.8.0 () … 独立したこの文書を用意。

---
[@isaki68k](https://misskey.io/@isaki68k)