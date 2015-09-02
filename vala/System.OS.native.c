#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/ttycom.h>

int
native_ioctl_TIOCGWINSZ(int fd, struct winsize *ws)
{
	return ioctl(fd, TIOCGWINSZ, ws);
}

int
native_sysctlbyname(const char *sname,
	void *oldp, size_t *oldlenp,
	const void *newp, size_t newlen)
{
	return sysctlbyname(sname, oldp, oldlenp, newp, newlen);
}
