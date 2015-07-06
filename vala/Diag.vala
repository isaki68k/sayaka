
// デバッグ用診断ツール
public class Diag
{
	public static bool global_trace = false;
	public static bool global_debug = false;
	public static int global_errexit = 1;

	public bool opt_trace;
	public bool opt_debug;
	public int opt_errexit;
	public string ClassName;

	public Diag(string className)
	{
		ClassName = className;
		opt_trace = global_trace;
		opt_debug = global_debug;
		opt_errexit = global_errexit;
	}

	public void Trace(string s)
	{
		if (opt_trace) {
			stderr.puts(@"$(ClassName) $(s)\n");
		}
	}

	public void Debug(string s)
	{
		if (opt_debug) {
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
}

