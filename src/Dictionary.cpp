#include "Dictionary.h"

#if defined(SELFTEST)
#include "test.h"
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
#endif // SELFTEST
