extern int native_ioctl_TIOCGWINSZ(int fd, out System.OS.winsize ws);
extern int native_sysctlbyname(string sname,
	void *oldp, size_t *oldlenp,
	void *newp, size_t newlen);

[CCode(cname="SIGWINCH")]
extern const int native_SIGWINCH;

namespace System.OS
{
	public struct winsize {
		ushort ws_row;
		ushort ws_col;
		ushort ws_xpixel;
		ushort ws_ypixel;
	}

	public class ioctl
	{
		public static int TIOCGWINSZ(int fd, out winsize ws)
		{
			return native_ioctl_TIOCGWINSZ(fd, out ws);
		}
	}

	public const int SIGWINCH = native_SIGWINCH;

	public class sysctl
	{
		public static int getbyname_int(string sname, out int oldp)
		{
			size_t oldlenp = sizeof(int);

			oldp = 0;
			return native_sysctlbyname(sname, &oldp, &oldlenp,
				null, 0);
		}
	}
}
