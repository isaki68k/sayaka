twitter クライアント sayaka ちゃん (vala版)
======

vala版に必要なもの
---
* vala-0.28 くらい以上
* glib2
* gdk-pixbuf2
* (glib-networking)

pkgsrc なら
lang/vala, devel/glib2, graphics/gdk-pixbuf2, net/glib-networking
をインストールしてください。


インストール方法
---
展開したディレクトリ内だけで動作しますので、
適当なところに展開してください。
といいつつ後述の理由により ~/.sayaka/ に展開することを前提にしています。

コンパイルは以下のようにします。

```
% make vala-make2
% make
```

この後まだ書く予定。
