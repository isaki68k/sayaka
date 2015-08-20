//
// vala のソースの依存関係を確認してビルドしたりするツール
// Copyright (C) 2015 isaki@NetBSD.org
//

// 使い方
//	vala-make [-c <vala-cmd>] <srcs...>
//
//	<srcs...> について、拡張子 .vala のファイルが
//	拡張子 .c のファイルより新しいものが1つでもあれば、
//	"<vala-cmd> <vala-srcs>" コマンドを実行します。
//	<vala-cmd> のデフォルトは "valac"。

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

void usage(void) __attribute__((__noreturn__));

int debug;

int
main(int ac, char *av[])
{
	struct stat stv;
	struct stat stc;
	const char *vala_cmd;
	time_t target_mtime;
	int updated;
	int c;
	int i;
	int r;

	vala_cmd = "valac";
	updated = 0;

	while ((c = getopt(ac, av, "c:d")) != -1) {
		switch (c) {
		 case 'c':
			vala_cmd = optarg;
			break;
		 case 'd':
			debug = 1;
			break;
		 default:
			usage();
			break;
		}
	}
	ac -= optind;
	av += optind;

	if (vala_cmd[0] == '\0') {
		usage();
	}

	// srcs... についてタイムスタンプを比較
	for (i = 0; i < ac; i++) {
		const char *valafile = av[i];
		char cfile[PATH_MAX];

		// foo.vala -> foo.c
		strlcpy(cfile, valafile, sizeof(cfile));
		char *p = strstr(cfile, ".vala");
		if (p == NULL) {
			errx(1, "<srcs> should have .vala extension");
		}
		strcpy(p, ".c");
		if (debug) {
			printf("checking %s and %s\n", valafile, cfile);
		}

		r = stat(valafile, &stv);
		if (r == -1) {
			err(1, "stat: %s", valafile);
		}
		r = stat(cfile, &stc);
		if (r == -1) {
			if (errno != ENOENT) {
				err(1, "stat: %s", cfile);
			}
		}

		if (stv.st_mtime > stc.st_mtime) {
			updated = 1;
			break;
		}
	}
	if (debug) {
		printf("%supdated\n", updated ? "" : "no ");
	}

	// 更新なければここで終わり
	if (updated == 0) {
		return 0;
	}

	char cmd[1024];	// 適当
	strcpy(cmd, vala_cmd);
	for (i = 0; i < ac; i++) {
		strlcat(cmd, " ", sizeof(cmd));
		strlcat(cmd, av[i], sizeof(cmd));
	}
	if (debug) {
		printf("cmd=|%s|\n", cmd);
	}
	system(cmd);
	return 0;
}

void __attribute__((__noreturn__))
usage(void)
{
	printf("usage\n");
	exit(1);
}
