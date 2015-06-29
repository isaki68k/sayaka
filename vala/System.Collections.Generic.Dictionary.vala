namespace System.Collections.Generic
{
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

	public class Dictionary<TKey, TValue>
	{
		private Array<KeyValuePair<TKey, TValue>> arraydata = new Array<KeyValuePair<TKey, TValue>>();

		public CompareFunc<TKey> KeyComparer { get; set; }

		private int intCmp(int a, int b) {
			return a - b;
		}

		public Dictionary()
		{
			if (typeof(TKey) == typeof(string)) {
				KeyComparer = (CompareFunc<TKey>)strcmp;
			} else if (typeof(TKey) == typeof(int)) {
				KeyComparer = (CompareFunc<TKey>)intCmp;
			} else {
				// XXX:
				KeyComparer = (CompareFunc<TKey>)intCmp;
			}
		}

		public void Add(TKey key, TValue value)
	 	{
			// XXX 重複チェック
			if (IndexOf(key) >= 0) {
				return;
			}
			AddCore(key, value);
		}

		private void AddCore(TKey key, TValue value)
		{
			arraydata.append_val(new KeyValuePair<TKey, TValue>(key, value));
		}

		public KeyValuePair<TKey, TValue>? At(int index)
		{
			if (index >= 0 && index < Count) {
				return arraydata.data[index];
			}
			return null;
		}

		public bool ContainsKey(TKey key)
		{
			return IndexOf(key) != -1;
		}

		public int Count { get { return (int)arraydata.length; } }

		public TValue? get(TKey key)
		{
			int index = IndexOf(key);
			if (index >= 0) {
				return arraydata.data[index].Value;
			}
stderr.printf("get return null!!!\n");
			return null;
		}

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

		public void set(TKey key, TValue value)
		{
			int index = IndexOf(key);
			if (index >= 0) {
				arraydata.data[index].Value = value;
			} else {
				AddCore(key, value);
			}
		}

		public void Dump()
		{
			for (int i = 0; i < arraydata.length; i++) {
				stderr.printf("%d %s %p\n", i, (string)arraydata.data[i].Key, arraydata.data[i].Value);
			}
		}
	}
}

