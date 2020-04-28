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

public class Diag
{
	// 分類名
	private string classname;
	// レベル。指標と後方互換性を兼ねて
	// 0: なし
	// 1: デバッグ
	// 2: トレース(詳細)
	// としておく。
	private int debuglevel = 0;

	// コンストラクタ
	public Diag()
	{
		classname = "";
	}
	public Diag.name(string name_)
	{
		classname = name_;
		if (classname != "") {
			classname += " ";
		}
	}

	// デバッグレベルを lv に設定する
	public void SetLevel(int lv)
	{
		debuglevel = lv;
	}
	// デバッグレベル取得
	public int GetLevel()
	{
		return debuglevel;
	}

	// レベル可変のメッセージ (改行はこちらで付加する)
	public void Print(int lv, string msg)
	{
		if (debuglevel >= lv)
			stderr.puts(classname + msg + "\n");
	}

	// デバッグログ表示 (改行はこちらで付加する)
	public void Debug(string msg)
	{
		Print(1, msg);
	}

	// トレースログ表示 (改行はこちらで付加する)
	public void Trace(string msg)
	{
		Print(2, msg);
	}
}
