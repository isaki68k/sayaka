#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
	int height, width;
	struct winsize ws;

	height = width = -1;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != -1) {
		if (ws.ws_row != 0) {
			height = ws.ws_ypixel / ws.ws_row;
		}

		if (ws.ws_col != 0) {
			width = ws.ws_xpixel / ws.ws_col;
		}
	}

	switch (argc) {
	case 1:
		printf("%d, %d\n", height, width);
		break;
	case 2:
		if (*argv[1]++ == '-') {
			switch (*argv[1]) {
			case 'h':
				printf("%d\n", height);
				break;
			case 'w':
				printf("%d\n", width);
				break;
			case 'v':
				printf("Terminal Size: %dx%d (%dx%d)\nCell Height: %d\nCell Width: %d\n",
				       ws.ws_col, ws.ws_row,
				       ws.ws_xpixel, ws.ws_ypixel,
				       height, width);
			default:
				return 1;
			}
		}
		break;
	default:
		return 1;
	}

	return 0;
}
