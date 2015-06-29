namespace System.Collections.Generic
{
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

	// .Net の Dictionary に似せて作る。
	public class Dictionary<TKey, TValue>
	{
		private Array<KeyValuePair<TKey, TValue>> arraydata = new Array<KeyValuePair<TKey, TValue>>();

		public CompareFunc<TKey> KeyComparer { get; set; }

		private int intCmp(int a, int b) {
			return a - b;
		}

		public Dictionary()
		{
			// 比較演算子のオーバーロードも IComparer も vala はサポートしていないので、
			// ジェネリックの型チェックが甘いことを逆用して、可変比較演算を実装する。

			if (typeof(TKey) == typeof(string)) {
				KeyComparer = (CompareFunc<TKey>)strcmp;
			} else if (typeof(TKey) == typeof(int)) {
				KeyComparer = (CompareFunc<TKey>)intCmp;
			} else {
				// XXX:
				KeyComparer = (CompareFunc<TKey>)intCmp;
			}
		}

		private void AddCore(TKey key, TValue value)
		{
			arraydata.append_val(new KeyValuePair<TKey, TValue>(key, value));
		}

		// あれば更新、なければ追加
		public void AddOrUpdate(TKey key, TValue value)
	 	{
			int index = IndexOf(key);
			if (index >= 0) {
				arraydata.data[index].Value = value;
			} else {
				AddCore(key, value);
			}
		}


		// i 番目の KeyValuePair を返す。
		// enumerator を実装するのが大変なので、インデックス列挙可能にしてある。
		public KeyValuePair<TKey, TValue>? At(int index)
		{
			if (index >= 0 && index < Count) {
				return arraydata.data[index];
			}
			return null;
		}

		// キーがあれば true を返す。
		public bool ContainsKey(TKey key)
		{
			return IndexOf(key) != -1;
		}

		// 要素数を返す。
		public int Count { get { return (int)arraydata.length; } }

		// key の値を返す。なければ null を返す。
		public TValue? get(TKey key)
		{
			int index = IndexOf(key);
			if (index >= 0) {
				return arraydata.data[index].Value;
			}
			return null;
		}

		// key のインデックスを返す。なければ -1 を返す。
		public int IndexOf(TKey key)
		{
			// 線形探索だwwww
			var tmp = arraydata.data;
			for (int i = 0; i < Count; i++) {
				if (KeyComparer(tmp[i].Key, key) == 0) {
					return i;
				}
			}
			return -1;
		}

		// key の値を設定する。なければ追加する。
		public void set(TKey key, TValue value)
		{
			AddOrUpdate(key, value);
		}

		public void Dump()
		{
			stderr.printf("%s", DumpString());
		}
		public string DumpString()
		{
			var sb = new StringBuilder();
			for (int i = 0; i < arraydata.length; i++) {
				sb.append("%d %s %p\n".printf(i, (string)arraydata.data[i].Key, arraydata.data[i].Value));
			}
			return sb.str;
		}
	}
}

