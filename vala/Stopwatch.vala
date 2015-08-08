//-D STOPWATCH_DISABLE

public class Stopwatch
{
#if !STOPWATCH_DISABLE
	private TimeVal tv_start;
	private TimeVal tv_stop;
#endif

	public void Start()
	{
#if !STOPWATCH_DISABLE
		tv_start.get_current_time();
#endif
	}

	public void Stop()
	{
#if !STOPWATCH_DISABLE
		tv_stop.get_current_time();
#endif
	}

	public void StopLog(string msg)
	{
#if !STOPWATCH_DISABLE
		Stop();
		stderr.printf("%s %"+ int64.FORMAT + "us\n", msg, Elapsed);
#endif
	}

	public int64 Elapsed
	{
		get {
#if !STOPWATCH_DISABLE
			return (tv_stop.tv_sec - tv_start.tv_sec) * 1000000 + tv_stop.tv_usec - tv_start.tv_usec;
#else
			return 0;
#endif
		}
	}
}
