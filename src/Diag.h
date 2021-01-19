#pragma once

#include <string>

class Diag
{
	// 分類名
	std::string classname {};

	// レベル。目安と後方互換製を兼ねて
	// 0: なし
	// 1: デバッグ
	// 2: トレース
	// 3: うるさい
	// としておく。
	int debuglevel {};

 public:
	Diag();
	Diag(const std::string& name);

	void SetClassname(const std::string& name);

	// デバッグレベルを設定する
	void SetLevel(int lv);
	// デバッグレベル取得
	int GetLevel() const { return debuglevel; }

	// (int) 評価するとデバッグレベルを返すと便利
	operator int() const { return GetLevel(); }

	// レベル可変のメッセージ出力 (改行はこちらで付加する)
	void Print(int lv, const char *fmt, ...);

	// レベル不問のメッセージ出力 (改行はこちらで付加する)
	// 呼び出し側でレベルを判定した後で使う
	void Print(const char *fmt, ...);

	// デバッグログ表示 (改行はこちらで付加する)
	void Debug(const char *fmt, ...);
	// トレースログ表示 (改行はこちらで付加する)
	void Trace(const char *fmt, ...);
	// 詳細ログ表示 (改行はこちらで付加する)
	void Verbose(const char *fmt, ...);
};
