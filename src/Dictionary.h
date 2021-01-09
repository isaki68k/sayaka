#pragma once

#include <map>
#include <string>

//
// Dictionary
//
// vala ではほぼ手作りしないといけなかったのでクラスとして用意してあるが
// C++ ではほぼ std::map でよい。
//
template <typename TKey, typename TValue>
class Dictionary
	: public std::map<TKey, TValue>
{
 public:
	// なければ追加、あれば何もしない
	void AddIfMissing(TKey key, TValue value)
	{
		this->try_emplace(key, value);
	}

	// なければ追加、あれば更新
	void AddOrUpdate(TKey key, TValue value)
	{
		this->insert_or_assign(key, value);
	}

	// 削除
	void Remove(TKey key)
	{
		this->erase(key);
	}

	// クリア
	void Clear()
	{
		this->clear();
	}

	// キーがあれば true を返す
	bool ContainsKey(TKey key) const
	{
		return (this->count(key) != 0);
	}

	// 要素数を返す
	int Count() const
	{
		return this->size();
	}
};

// よく使うのでショートカット
using StringDictionary = Dictionary<std::string, std::string>;

#if defined(SELFTEST)
extern void test_Dictionary();
#endif
