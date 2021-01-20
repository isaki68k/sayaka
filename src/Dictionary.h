/*
 * Copyright (C) 2021 Tetsuya Isaki
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
