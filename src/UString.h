#include "StringUtil.h"
#include <vector>

using unichar = uint32_t;

// Unicode コードポイントの配列。
// UString とは言っているが文字列ではなく vector 派生なことに留意。
// 主に std::vector に append() がないのでこんなことになっている…。
class UString : public std::vector<unichar>
{
	using inherited = std::vector<unichar>;
	using size_type = std::size_t;
 public:
	// コンストラクタ
	explicit UString()				// デフォルトコンストラクタ
		: inherited() { }
	UString(const UString& s)		// コピーコンストラクタ
		: inherited(s) { }
	UString(UString&& s) noexcept	// ムーブコンストラクタ
		: inherited(s) { }
	UString(std::initializer_list<unichar> il)	// 初期化子リストを受け取る
		: inherited(il) { }

	// 代入演算子
	UString& operator=(const UString& s) {
		inherited::operator=(s);
		return *this;
	}

	// UString s を末尾に追加
	UString& operator+=(const UString& s) {
		Append(s);
	}
	// unichar u を末尾に追加
	UString& operator+=(unichar u) {
		emplace_back(u);
	}

	// UString s を末尾に追加
	void Append(const UString& s) {
		insert(end(), s.begin(), s.end());
	}
	// unichar u を末尾に追加
	void Append(unichar u) {
		emplace_back(u);
	}

	// 文字列 s (ASCII) を1文字ずつ末尾に追加
	// (ASCII またはエスケープシーケンスのみの場合に使用できる)
	void AppendChars(const std::string& s) {
		for (auto c : s) {
			emplace_back(c);
		}
	}

	// 文字列 s (UTF-8) を UString に変換して末尾に追加
	void Append(const std::string& s);
};

// 文字列との相互変換
extern UString StringToUString(const std::string& str);
extern std::string UStringToString(const UString& ustr);

#if defined(SELFTEST)
extern void test_UString();
#endif
