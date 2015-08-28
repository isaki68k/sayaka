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
//
//	-e はコマンドを実行する際にそのコマンドラインを表示します。
//	make で @./vala-make と組み合わせるとそれっぽくなるかも。

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
int dry_run;
int echocmd;

int
main(int ac, char *av[])
{
	char cmd[1024];	// 適当
	const char *vala_cmd;
	const char *cc_cmd;
	const char *ld_cmd;
	const char *libs;
	const char *exefile;
	char **cfiles;
	char **ofiles;
	time_t target_mtime;
	int updated;
	int *updates;
	int c;
	int i;
	int r;

	vala_cmd = "valac -C";
	cc_cmd = "cc";
	ld_cmd = "cc";
	libs = "";
	exefile = "a.out";
	updated = 0;
	dry_run = 0;
	echocmd = 0;

	while ((c = getopt(ac, av, "a:c:del:L:no:")) != -1) {
		switch (c) {
		 case 'a':
			vala_cmd = optarg;
			break;
		 case 'c':
			cc_cmd = optarg;
			break;
		 case 'd':
			debug = 1;
			break;
		 case 'e':
			echocmd = 1;
			break;
		 case 'l':
			ld_cmd = optarg;
			break;
		 case 'L':
			libs = optarg;
			break;
		 case 'n':
			dry_run = 1;
			break;
		 case 'o':
			exefile = optarg;
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

	cfiles = malloc(ac * sizeof(char*));
	if (cfiles == NULL) {
		err(1, "malloc");
	}
	ofiles = malloc(ac * sizeof(char*));
	if (ofiles == NULL) {
		err(1, "malloc");
	}
	updates = calloc(ac, sizeof(int));
	if (updates == NULL) {
		err(1, "malloc");
	}

	// .vala と同じだけの .c と .o ファイル名を作成
	for (i = 0; i < ac; i++) {
		const char *valafile = av[i];
		char filename[PATH_MAX];

		// foo.vala -> foo.c
		strlcpy(filename, valafile, sizeof(filename));
		char *p = strstr(filename, ".vala");
		if (p == NULL) {
			errx(1, "<srcs> should have .vala extension");
		}

		strcpy(p, ".c");
		cfiles[i] = strdup(filename);

		strcpy(p, ".o");
		ofiles[i] = strdup(filename);
	}

	//
	// .vala -> .c
	//

	// .vala vs .c についてタイムスタンプをすべて比較
	// どの C ソースが更新されたか個別に把握するので、早期終了しない。
	updated = 0;
	for (i = 0; i < ac; i++) {
		const char *valafile = av[i];
		const char *cfile = cfiles[i];

		if (need_update(valafile, cfile) == 1) {
			updates[i] = 1;
			updated++;
		}
	}

	// 必要なら valac 実行
	if (updated) {
		strcpy(cmd, vala_cmd);
		for (i = 0; i < ac; i++) {
			strlcat(cmd, " ", sizeof(cmd));
			strlcat(cmd, av[i], sizeof(cmd));
		}
		run_cmd(cmd);

		// .c のタイムスタンプを更新
		for (i = 0; i < ac; i++) {
			if (updates[i]) {
				if (debug) {
					printf("touching %s\n", cfiles[i]);
				}
				if (utimes(cfiles[i], NULL) == -1) {
					err(1, "utimes: %s", cfiles[i]);
				}
			}
		}
	}

	//
	// .c -> .o
	//

	// タイムスタンプを比較して、それぞれコンパイル
	updated = 0;
	for (i = 0; i < ac; i++) {
		updates[i] = 0;
	}
	for (i = 0; i < ac; i++) {
		const char *cfile = cfiles[i];
		const char *ofile = ofiles[i];

		if (need_update(cfile, ofile) == 1) {
			snprintf(cmd, sizeof(cmd), "%s -c %s -o %s", cc_cmd, cfile, ofile);
			run_cmd(cmd);
		}
	}

	//
	// .o -> target_exe
	//

	// .o と exe を比較
	updated = 0;
	for (i = 0; i < ac; i++) {
		const char *ofile = ofiles[i];
		if (need_update(ofile, exefile) == 1) {
			updated++;
			break;
		}
	}
	if (updated) {
		sprintf(cmd, "%s -o %s", ld_cmd, exefile);
		for (i = 0; i < ac; i++) {
			strlcat(cmd, " ", sizeof(cmd));
			strlcat(cmd, ofiles[i], sizeof(cmd));
		}
		strlcat(cmd, " ", sizeof(cmd));
		strlcat(cmd, libs, sizeof(cmd));

		run_cmd(cmd);
	}

	return 0;
}

// srcfile が dstfile より新しければ 1 を返す。
int
need_update(const char *srcfile, const char *dstfile)
{
	struct stat st1;
	struct stat st2;
	int r;

	if (debug) {
		printf("checking %s and %s .. ", srcfile, dstfile);
		fflush(stdout);
	}

	r = stat(srcfile, &st1);
	if (r == -1) {
		err(1, "stat: %s", srcfile);
	}

	memset(&st2, 0, sizeof(st2));
	r = stat(dstfile, &st2);
	if (r == -1) {
		if (errno != ENOENT) {
			err(1, "stat: %s", dstfile);
		}
	}

	if (st1.st_mtime > st2.st_mtime) {
		r = 1;
	}
	if (debug) {
		if (r) {
			printf("need-to-update\n");
		} else {
			printf("no-update\n");
		}
	}
	return r;
}

void
run_cmd(const char *cmd)
{
	if (dry_run) {
		printf("%s\n", cmd);
	} else {
		if (echocmd) {
			printf("%s\n", cmd);
		}
		if (system(cmd) != 0) {
			exit(1);
		}
	}
}

void __attribute__((__noreturn__))
usage(void)
{
	printf("usage\n");
	exit(1);
}
