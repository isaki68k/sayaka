
# gnumake か bsdmake かを判別して include ファイルの拡張子を切り替える。
# gnumake では MAKE_COMMAND という変数が定義される。
# bsdmake ではこの変数はないので、判別できる(?)

ISGNU=bsd${MAKE_COMMAND:%=gnu}
MAKEEXT=${ISGNU:bsdgnu=gnu}

# include 文の引数をクォートしなければ、どちらでも動く

include Makefile.${MAKEEXT}

