#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/ttycom.h>

int
native_ioctl_TIOCGWINSZ(int fd, struct winsize *ws)
{
	return ioctl(fd, TIOCGWINSZ, ws);
}
