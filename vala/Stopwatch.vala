//-D STOPWATCH_DISABLE

public class Stopwatch
{
#if !STOPWATCH_DISABLE
	private TimeVal tv_start;
	private TimeVal tv_stop;
	private TimeVal tv_total;
#endif

	public void Reset()
	{
#if !STOPWATCH_DISABLE
		tv_total.tv_sec = 0;
		tv_total.tv_usec = 0;
#endif
	}

	public void Restart()
	{
		Reset();
		Start();
	}

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

		// timersub: result = stop - start
		TimeVal result = TimeVal();
		result.tv_sec  = tv_stop.tv_sec  - tv_start.tv_sec;
		result.tv_usec = tv_stop.tv_usec - tv_start.tv_usec;
		if (result.tv_usec < 0) {
			result.tv_sec--;
			result.tv_usec += 1000000;
		}

		// timeradd: total = total + result
		tv_total.tv_sec  = tv_total.tv_sec  + result.tv_sec;
		tv_total.tv_usec = tv_total.tv_usec + result.tv_usec;
		if (tv_total.tv_usec >= 1000000) {
			tv_total.tv_sec++;
			tv_total.tv_usec -= 1000000;
		}
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
			return tv_total.tv_sec * 1000000 + tv_total.tv_usec;
#else
			return 0;
#endif
		}
	}
}
