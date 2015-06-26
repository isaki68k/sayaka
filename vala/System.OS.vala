extern int native_ioctl_TIOCGWINSZ(int fd, out System.OS.winsize ws);

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
}
