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

