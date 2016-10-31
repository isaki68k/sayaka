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

// デバッグ用診断ツール

// グローバル
public class gDiag
{
	public static bool global_trace = false;
	public static bool global_debug = false;
	public static bool global_warn  = false;
	public static bool global_error = true;

	public static int global_errexit = 1;

	public static void Puts(string s, string className = "")
	{
		TimeVal tv = TimeVal();
		DateTime dt = new DateTime.from_timeval_local(tv);
		var time = dt.format("%T") + ".%03d".printf((int)tv.tv_usec / 1000);
		if (className == "") {
			stderr.puts(@"[$(time)] $(s)\n");
		} else {
			stderr.puts(@"[$(time)] $(className) $(s)\n");
		}
	}

	public static void PutHex(string s, uchar[] d, string className = "")
	{
		Puts(s, className);
		int len = d.length;
		for (int i = 0; i < len; i++) {
			stderr.printf(" %02X", d[i]);
			if (i % 16 == 15 && i < len - 1) {
				stderr.putc('\n');
			}
		}
		stderr.putc('\n');
	}

	public static void Trace(string s)
	{
		if (global_trace) {
			Puts(s);
		}
	}

	public static void Debug(string s)
	{
		if (global_debug) {
			Puts(s);
		}
	}

	public static void Warn(string s)
	{
		if (global_warn) {
			Puts(s);
		}
	}

	public static void Error(string s)
	{
		if (global_error) {
			Puts(s);
			if (global_errexit > 0) {
				Process.exit(global_errexit);
			}
		}
	}

	public static void DebugHex(string s, uchar[] d)
	{
		if (global_debug) {
			PutHex(s, d);
		}
	}

	public static void PROGERR(string s)
	{
		Puts(s, "PROGERR!!");
		if (global_errexit > 0) {
			Process.exit(global_errexit);
		}
	}
}

// デバッグ用診断ツール
public class Diag
{
	public bool opt_trace;
	public bool opt_debug;
	public bool opt_warn;
	public bool opt_error;
	public int opt_errexit;
	public string ClassName;

	public Diag(string className)
	{
		ClassName = className;
		opt_trace = gDiag.global_trace;
		opt_debug = gDiag.global_debug;
		opt_warn = gDiag.global_warn;
		opt_error = gDiag.global_error;
		opt_errexit = gDiag.global_errexit;
	}

	public void Trace(string s)
	{
		if (opt_trace) {
			gDiag.Puts(s, ClassName);
		}
	}

	public void Debug(string s)
	{
		if (opt_debug) {
			gDiag.Puts(s, ClassName);
		}
	}

	public void Warn(string s)
	{
		if (opt_warn) {
			gDiag.Puts(s, ClassName);
		}
	}

	public void Error(string s)
	{
		if (opt_error) {
			gDiag.Puts(s, ClassName);
		}
	}

	public void DebugHex(uchar[] d, int len)
	{
		if (opt_debug) {
			gDiag.PutHex("", d, ClassName);
		}
	}

	// errIfTrue が true の時エラーメッセージを出力します。
	// [Obsolete]
	public void ProgErr(bool errIfTrue, string s)
	{
		if (errIfTrue) {
			stderr.puts(@"$(ClassName) PROGERR!! $(s)\n");
			if (opt_errexit > 0) {
				Process.exit(opt_errexit);
			}
		}
	}

	// [Obsolete]
	public static void GlobalProgErr(bool errIfTrue, string s)
	{
		if (errIfTrue) {
			gDiag.PROGERR(s);
		}
	}
}

