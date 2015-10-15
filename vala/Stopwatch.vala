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

//-D STOPWATCH_DISABLE

public class Stopwatch
{
#if STOPWATCH_DISABLE

	public int Count;
	public int64 Elapsed;
	public Stopwatch() { }
	public void Reset() { }
	public void Start() { }
	public void Stop() { }
	public void StopLog() { }

	
#else

	private DateTime start;
	private DateTime stop;
	private TimeSpan total;

	// 何回 Stop() を通過したかを保持するカウンタ。
	// .Net の Stopwatch にはないけど、プロファイラを作るため。
	public int Count {
		get {
			return Count_;
		}
		protected set {
			Count_ = value;
		}
	}
	private int Count_;

	public int64 Elapsed
	{
		get {
			return (int64)total;
		}
	}

	public Stopwatch()
	{
		Reset();
	}

	public void Reset()
	{
		total = 0;
		Count = 0;
	}

	public void Restart()
	{
		Reset();
		Start();
	}

	public void Start()
	{
		start = new DateTime.now_local();
	}

	public void Stop()
	{
		stop = new DateTime.now_local();

		TimeSpan result = stop.difference(start);

		total += result;

		Count_++;
	}

	public void StopLog(string msg)
	{
		Stop();
		stderr.printf("%s %"+ int64.FORMAT + "us\n", msg, Elapsed);
	}

#endif
}

