namespace ULib
{
	// GLib.OptionContext はそれほど悪くないと思うが、
	// ちょっと違う。


	public class OptArgs
	{
		private string[] Args;

		public OptArgs(string[] args)
		{
//stdout.printf("OptArgs.ctor args.length=%d ", args.length);
			Args = args;
		}

		public string GetArg(int Index)
		{
			return Args[Index];
		}

		public int size {
			get {
//stdout.printf("OptArgs.size=%d ", Args.length);
				return Args.length;
			}
		}


	/* Iterator はダックタイピングらしい */

		public Iterator iterator()
		{
//stdout.printf("iterator() ");
			return new Iterator(this);
		}

		public class Iterator
		{
			private int Index;
			private OptArgs Owner;
			
			public Iterator(OptArgs owner)
			{
//stdout.printf("iterator.ctor() owner.size=%d ", owner.size);
				// Args[0] はプログラム自身なので、本来先頭からなら -1 を
				// セットするところ、0 にして、[1] から列挙させる。
				Index = 0;
				Owner = owner;
			}

			public bool next()
	 		{
//stdout.printf("iterator.next() ");
				Index++;
				return Index < Owner.size;
			}

			public OptArg @get()
			{
//stdout.printf("iterator.get() ");
				return new OptArg(this, GetArg());
			}


			public string GetArg() { return Owner.GetArg(Index); }
		}
	}

	public class OptArg
	{
		private OptArgs.Iterator Owner;
		private string Arg;
		private string? ValueString_ = null;

		public OptArg(OptArgs.Iterator owner, string arg)
		{
			Owner = owner;
			Arg = arg;
		}

		public bool IsOption()
		{
			return Arg.length > 0 && Arg[0] == '-';
		}

		public string Opt()
		{
			if (IsOption() == false) {
				return "";
			}

			int p = Arg.index_of("=");
			if (p > 0) {
				ValueString_ = Arg.substring(p + 1);
				return Arg.substring(0, p);
			}

			return Arg;
		}

		public string ValueFromNext()
		{
			if (Owner.next()) {
				ValueString_ = Owner.GetArg();
				return ValueString_;
			} else {
				return "";
			}
		}

		public string ValueString()
		{
			if (ValueString_ != null) {
				return ValueString_;
			}

			if (IsOption() == false) {
				return Arg;
			}

			// 1 char option
			if (Arg.length == 2) {
				return ValueFromNext();
			}

			return ValueFromNext();
			//return "";
		}

		public int ValueInt()
		{
			return int.parse(ValueString());
		}
	}
}
