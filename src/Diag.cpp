// デバッグ用診断ツール

#include "Diag.h"
#include <cstdio>
#include <cstdarg>

// コンストラクタ
Diag::Diag()
{
}

// コンストラクタ
Diag::Diag(const std::string& name_)
{
	SetClassname(name_);
}

// クラス名を後から設定する
void
Diag::SetClassname(const std::string& name_)
{
	classname = name_;
	if (!classname.empty()) {
		classname += " ";
	}
}

// デバッグレベルを lv に設定する
void
Diag::SetLevel(int lv)
{
	debuglevel = lv;
}

#define VPRINTF(fmt) do { \
	va_list ap; \
	va_start(ap, (fmt)); \
	vfprintf(stderr, (fmt), ap); \
	fputs("\n", stderr); \
	va_end(ap); \
} while (0)

// レベル不問のメッセージ (改行はこちらで付加する)
void
Diag::Print(const char *fmt, ...)
{
	VPRINTF(fmt);
}

// レベル可変のメッセージ (改行はこちらで付加する)
void
Diag::Print(int lv, const char *fmt, ...)
{
	if (debuglevel >= lv) {
		VPRINTF(fmt);
	}
}

// デバッグログ表示 (改行はこちらで付加する)
void
Diag::Debug(const char *fmt, ...)
{
	if (debuglevel >= 1) {
		VPRINTF(fmt);
	}
}

// トレースログ表示 (改行はこちらで付加する)
void
Diag::Trace(const char *fmt, ...)
{
	if (debuglevel >= 2) {
		VPRINTF(fmt);
	}
}
