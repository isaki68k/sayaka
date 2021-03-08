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

#include "test.h"
#include "Dictionary.h"

void
test_Dictionary()
{
	printf("%s\n", __func__);

	// create
	StringDictionary dict;
	xp_eq(dict.Count(), 0);

	// AddIfMissing
	dict.AddIfMissing("aaa", "a");
	xp_eq(dict.Count(), 1);
	// なければ追加
	dict.AddIfMissing("bbb", "b");
	xp_eq(dict.Count(), 2);
	// あるので何もしない
	dict.AddIfMissing("aaa", "a");
	xp_eq(dict.Count(), 2);
	xp_eq(dict["aaa"], "a");
	// 値だけ違うキーも更新にはならない
	dict.AddIfMissing("aaa", "x");
	xp_eq(dict.Count(), 2);
	xp_eq(dict["aaa"], "a");

	// AddOrUpdate
	// 同じキーと値なら実質変わらない
	dict.AddOrUpdate("aaa", "a");
	xp_eq(dict.Count(), 2);
	xp_eq(dict["aaa"], "a");
	// 値を更新
	dict.AddOrUpdate("aaa", "x");
	xp_eq(dict.Count(), 2);
	xp_eq(dict["aaa"], "x");
	// 値を追加
	dict.AddOrUpdate("ccc", "c");
	xp_eq(dict.Count(), 3);
	xp_eq(dict["ccc"], "c");

	// Remove
	dict.Remove("aaa");
	xp_eq(dict.Count(), 2);
	dict.Remove("aaa");
	xp_eq(dict.Count(), 2);

	// Clear
	dict.Clear();
	xp_eq(dict.Count(), 0);
}
