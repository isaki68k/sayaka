/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
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

namespace ULib
{
	// ----- KeyValuePair

	// .Net の KeyValuePair に似せて作る。
	public class KeyValuePair<TKey, TValue>
	{
		public TKey? Key { get { return Key_; } }
		private TKey Key_;

		public TValue? Value {
			get { return Value_; }
			set { Value_ = value; }
		}
		private TValue Value_;

		public KeyValuePair(TKey key, TValue value)
		{
			Key_ = key;
			Value_ = value;
		}
	}

	// ----- キー比較関数

	// namespace static function
	public static int IntCmp(int a, int b)
	{
		return a - b;
	}

	// namespace static function
	public static int Int64Cmp(int64 a, int64 b)
	{
		// (int)(a - b) ではオーバーフローする

		if (a > b) return -1;
		if (a < b) return 1;
		return 0;
	}

	// ソート済み配列
	public class SortedArray<T>
	{
		//private Diag diag = new Diag("SortedArray");
		private GenericArray<T> data;
		public CompareDataFunc<T> Comparer;

#if false
		// 本当はこう書きたいが、vala がジェネリックの中のジェネリックを
		// うまくコンパイル出来ないので、コンストラクタに呼び出し側が
		// 生成したオブジェクトを渡さないとコンパイル出来ない。
		// オワットル
		public SortedArray(owned CompareDataFunc<T> comparer)
		{
			data = new GenericArray<T>();
			Comparer = (owned) comparer;
		}
#endif

		public SortedArray.backend(owned CompareDataFunc<T> comparer,
			owned GenericArray<T> backend)
		{
			data = (owned) backend;
			Comparer = (owned) comparer;
		}

		// 値を追加します。
		public void Add(T value)
		{
			int i = IndexOf(value);
			if (i < 0) {
				i = ~i;
			}
			//diag.Debug(@"Add $(i)");
			data.insert(i, value);
		}

		// クリアします。
		public void Clear()
		{
			if (Count > 0) {
				data.remove_range(0, Count);
			}
		}

		// 要素数を返します。
		public int Count { get { return data.length; } }

		// FindIndex 用のデリゲートです。
		// item には配列内のデータが渡されます。
		// strcmp(item, "criteria") を想定しています。
		public delegate int FindFunc<T>(T item);

		// 検索関数を使用して検索し、インデックスを返します。
		// 見つからない時は挿入するべき位置の not を返します。
		// 検索関数が一貫していない時の動作は、不定です。
		public int FindIndex(FindFunc<T> finder)
		{
			int L = 0;
			int R = Count - 1;
			int M = 0;
			int c = 0;
			while (L <= R) {
				M = (L + R) / 2;
				c = finder(data[M]);
				if (c == 0) {
					//diag.Debug(@"FindIndex c=0 M=$(M)");
					return M;
				}
				if (c < 0) {
					L = M + 1;
				} else {
					R = M - 1;
				}
			}
			// 挿入するべき位置を not で返す
			if (c < 0) {
				//diag.Debug(@"FindIndex c<0 M=$(M)");
				return ~(M + 1);
			} else {
				//diag.Debug(@"FindIndex c>=0 M=$(M)");
				return ~(M);
			}
		}

		// value を検索してインデックスを返します。
		// 見つからない時は挿入するべき位置の not を返します。
		public int IndexOf(T value)
		{
			//diag.Debug("IndexOf");
			return FindIndex((item) => Comparer(item, value));
		}

		// ソートを保ったインデックスを呼び出し側が保証して下さい。
		public void InternalInsert(int index, T value)
		{
			data.insert(index, value);
		}

		// ----- iterator support

		public T get(int index)
		{
			return data[index];
		}

		public void set(int index, T value)
		{
			data[index] = value;
		}

		public int size { get { return Count; } }
	}

	// ----- Dictionary

	// いろいろ考えた。IDictionary インタフェース用意するかとか、
	// 継承使ってソート条件指定するかとか。でも、コンパイル時間や
	// オブジェクトコードサイズを考えてシンプルにすることにした。
	// .Net の Dictionary に似せて作る。
	// 中身は SortedDictionary にする。O(logN) で上等。
	public class Dictionary<TKey, TValue>
	{
		//private Diag diag = new Diag("Dictionary");
		private SortedArray<KeyValuePair<TKey, TValue>> data;

		public CompareFunc<TKey> KeyComparer { get; set; }

		// KeyValuePair の キーで比較する
		public int KeyValuePairKeyCmp(KeyValuePair<TKey, TValue> a,
			KeyValuePair<TKey, TValue> b)
		{
			return KeyComparer(a.Key, b.Key);
		}

		public Dictionary()
		{
			// 比較演算子のオーバーロードも IComparer も vala はサポート
			// していないので、ジェネリックの型チェックが甘いことを逆用して
			// 可変比較演算を実装する。

			if (typeof(TKey) == typeof(string)) {
				KeyComparer = (CompareFunc<TKey>)strcmp;
			} else if (typeof(TKey) == typeof(int)) {
				KeyComparer = (CompareFunc<TKey>)IntCmp;
			} else {
				stderr.printf("Dictionary::Dictionary(): TKey not supported");
				assert(false);
			}
			// コンパイラのバグ回避
			data = new SortedArray<KeyValuePair<TKey, TValue>>.backend(
				KeyValuePairKeyCmp,
				new GenericArray<KeyValuePair<TKey, TValue>>());
		}

		// なければ追加、あれば何もしない
		public void AddIfMissing(TKey key, TValue value)
		{
			int index = data.FindIndex((item) => KeyComparer(item.Key, key));
			if (index >= 0) {
				return;
			} else {
				data.InternalInsert(~index,
					new KeyValuePair<TKey, TValue>(key, value));
			}
		}

		// あれば更新、なければ追加
		public void AddOrUpdate(TKey key, TValue value)
	 	{
			int index = data.FindIndex((item) => KeyComparer(item.Key, key));
			if (index >= 0) {
				data[index].Value = value;
			} else {
				data.InternalInsert(~index,
					new KeyValuePair<TKey, TValue>(key, value));
			}
		}

		// i 番目の KeyValuePair を返す。
		// enumerator を実装するのが大変なので、インデックス列挙可能にしてある。
		public KeyValuePair<TKey, TValue>? At(int index)
		{
			if (index >= 0 && index < Count) {
				return data[index];
			}
			return null;
		}

		// クリアします。
		public void Clear()
		{
			data.Clear();
		}

		// キーがあれば true を返す。
		public bool ContainsKey(TKey key)
		{
			return IndexOf(key) != -1;
		}

		// 要素数を返す。
		public int Count { get { return data.Count; } }

		// key のインデックスを返す。なければ -1 を返す。
		public int IndexOf(TKey key)
		{
			//diag.Debug("IndexOf");
			int index = data.FindIndex((item) => KeyComparer(item.Key, key));
			if (index >= 0) return index;
			return -1;
		}

		// ソートされているかどうかを返します。
		public bool IsSorted { get { return true; } }

		// ----- iterator support

		// key の値を返す。なければ null を返す。
		public TValue? get(TKey key)
		{
			int index = IndexOf(key);
			if (index >= 0) {
				return data[index].Value;
			}
			return null;
		}

		// key の値を設定する。なければ追加する。
		public void set(TKey key, TValue value)
		{
			AddOrUpdate(key, value);
		}

		// size プロパティがあると get - size タイプのイテレータとみなされる
		// Dictionary は get の引数型が int じゃないので怒られる。
		//public int size { get { return Count; } }

		public DictionaryIter iterator()
		{
			return new DictionaryIter(this);
		}

		// この内部クラスにジェネリック型を付加するとコンパイルが通らない。
		public class DictionaryIter
		{
			private Dictionary Owner;
			private int Index;

			public DictionaryIter(Dictionary owner)
			{
				Owner = owner;
				Index = -1;
			}

			public KeyValuePair? next_value()
			{
				Index++;
				if (Index < Owner.Count) {
					return Owner.At(Index);
				} else {
					return null;
				}
			}
		}

		// -----
		public string to_string()
		{
			var sb = new StringBuilder();
			for (int i = 0; i < Count; i++) {
				sb.append("%s%s=%s".printf((i == 0) ? "" : ", ",
					(string)data[i].Key,
					(string)data[i].Value));
			}
			return sb.str;
		}

		// ----- debug

		public void Dump()
		{
			stderr.printf("%s", DumpString());
		}
		public string DumpString()
		{
			var sb = new StringBuilder();
			for (int i = 0; i < Count; i++) {
				sb.append("[%d] |%s|=|%s|\n".printf(i,
					(string)data[i].Key,
					(string)data[i].Value));
			}
			return sb.str;
		}
	}
}
