//
// vala のソースの依存関係を確認してビルドしたりするツール
// Copyright (C) 2015 isaki@NetBSD.org
//

// 使い方
//	vala-make -o <target> [-c <vala-cmd>] <vala-srcs...>
//
//	<vala-srcs...> のいずれか一つでもが <target> より新しければ
//	"<vala-cmd> <vala-srcs>" コマンドを実行します。
//	<vala-cmd> のデフォルトは "valac"。

#include <err.h>
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
	struct stat st;
	const char *target_file;
	const char *vala_cmd;
	time_t target_mtime;
	int updated;
	int c;
	int i;
	int r;

	target_file = NULL;
	vala_cmd = "valac";
	updated = 0;

	while ((c = getopt(ac, av, "c:do:")) != -1) {
		switch (c) {
		 case 'c':
			vala_cmd = optarg;
			break;
		 case 'd':
			debug = 1;
			break;
		 case 'o':
			target_file = optarg;
			break;
		 default:
			usage();
			break;
		}
	}
	ac -= optind;
	av += optind;

	if (target_file == NULL) {
		usage();
	}
	if (vala_cmd[0] == '\0') {
		usage();
	}

	// target_file のタイムスタンプを取得
	r = stat(target_file, &st);
	if (r == -1) {
		err(1, "stat: %s", target_file);
	}
	target_mtime = st.st_mtime;
	if (debug) {
		printf("target_mtime = %u\n", (int)target_mtime);
	}

	// srcs... のタイムスタンプと比較
	for (i = 0; i < ac; i++) {
		const char *file = av[i];
		if (debug) {
			printf("checking %s\n", file);
		}
		r = stat(file, &st);
		if (r == -1) {
			err(1, "stat: %s", file);
		}
		if (st.st_mtime > target_mtime) {
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
