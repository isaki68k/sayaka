
// デバッグ用診断ツール
public class Diag
{
	public static bool global_trace = false;
	public static bool global_debug = false;
	public static bool global_warn  = false;
	public static bool global_error = true;

	public static int global_errexit = 1;

	public bool opt_trace;
	public bool opt_debug;
	public bool opt_warn;
	public bool opt_error;
	public int opt_errexit;
	public string ClassName;

	public Diag(string className)
	{
		ClassName = className;
		opt_trace = global_trace;
		opt_debug = global_debug;
		opt_warn = global_warn;
		opt_error = global_error;
		opt_errexit = global_errexit;
	}

	public void Trace(string s)
	{
		if (opt_trace) {
			TimeVal tv = TimeVal();
			DateTime dt = new DateTime.from_timeval_local(tv);
			var time = dt.format("%T") + ".%03d".printf((int)tv.tv_usec / 1000);
			stderr.puts(@"[$(time)] $(ClassName) $(s)\n");
		}
	}

	public void Debug(string s)
	{
		if (opt_debug) {
			TimeVal tv = TimeVal();
			DateTime dt = new DateTime.from_timeval_local(tv);
			var time = dt.format("%T") + ".%03d".printf((int)tv.tv_usec / 1000);
			stderr.puts(@"[$(time)] $(ClassName) $(s)\n");
		}
	}

	public void Warn(string s)
	{
		if (opt_warn) {
			stderr.puts(@"$(ClassName) $(s)\n");
		}
	}

	public void Error(string s)
	{
		if (opt_error) {
			stderr.puts(@"$(ClassName) $(s)\n");
		}
	}

	public void DebugHex(uchar[] d, int len)
	{
		if (opt_debug) {
			stderr.puts(@"$(ClassName)");
			for (int i = 0; i < len; i++) {
				stderr.printf(" %02X", d[i]);
				if (i % 16 == 15 && i < len - 1) {
					stderr.putc('\n');
				}
			}
			stderr.putc('\n');
		}
	}


	// errIfTrue が true の時エラーメッセージを出力します。
	public void ProgErr(bool errIfTrue, string s)
	{
		if (errIfTrue) {
			stderr.puts(@"$(ClassName) PROGERR!! $(s)\n");
			if (opt_errexit > 0) {
				Process.exit(opt_errexit);
			}
		}
	}

	public static void GlobalProgErr(bool errIfTrue, string s)
	{
		if (errIfTrue) {
			stderr.puts(@"PROGERR!! $(s)\n");
			if (global_errexit > 0) {
				Process.exit(global_errexit);
			}
		}
	}
}

