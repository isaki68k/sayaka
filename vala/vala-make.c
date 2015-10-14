//
// vala のソースの依存関係を確認してビルドしたりするツール
//

/*
 * Copyright (C) 2015 Tetsuya Isaki
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// 使い方
//	vala-make -o <exefile> [<options...>] <srcs...>
//
//	<srcs...> (.valaファイル) から実行ファイル <exefile> をビルドします。
//	その際各ファイル同士のタイムスタンプを比較し、必要なコマンドだけを
//	実行するように努めます。
//
//	  -a <valac_cmd>
//		vala コンパイラのコマンド(とオプション)を指定します。
//		デフォルトは "valac -C" です。
//
//	  -c <cc_cmd>
//		C コンパイラのコマンド(とオプション)を指定します。
//		デフォルトは "cc" です。
//
//	  -d
//		デバッグ表示をします。
//
//	  -e
//		エコーモードです。コマンドを実行する際にそのコマンドラインを
//		表示します。make で @./vala-make と組み合わせるとそれっぽく
//		なるかも。
//
//	  -l <ld_cmd>
//		C リンカのコマンド(とオプション)を指定します。
//		デフォルトは "cc" です。
//
//	  -L <libs>
//		リンカに指定するライブラリ等を指定します。
//
//	  -n
//		dry-run です。実際には何も実行しません。
//		実行しなかったことによりタイムスタンプが更新されず、その後の
//		実行結果の表示が -n なしの時とは異なる場合がありますが、限界です。
//
//	  -o <exefile>
//		実行ファイル名です。
//
//	  -O <objdir>
//		中間ファイルである .c と .o を置くディレクトリを指定します。
//		デフォルトは "." (カレントディレクトリ) です。
//

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

void usage(void) __attribute__((__noreturn__));

void run_cmd(const char *);

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
	const char *objdir;
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
	objdir = NULL;

	while ((c = getopt(ac, av, "a:c:del:L:no:O:")) != -1) {
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
		 case 'O':
			objdir = optarg;
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
	// -O <objdir> なら valac にも -d <objdir> を追加
	if (objdir != NULL) {
		char tmp[1024];	// 適当
		snprintf(tmp, sizeof(tmp), "%s -d %s", vala_cmd, objdir);
		vala_cmd = strdup(tmp);
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
		char pathname[PATH_MAX];

		// foo.vala -> foo.c
		strlcpy(filename, valafile, sizeof(filename));
		char *p = strstr(filename, ".vala");
		if (p == NULL) {
			errx(1, "<srcs> should have .vala extension");
		}

		strcpy(p, ".c");
		if (objdir == NULL) {
			cfiles[i] = strdup(filename);
		} else {
			snprintf(pathname, sizeof(pathname), "%s/%s", objdir, filename);
			cfiles[i] = strdup(pathname);
		}

		strcpy(p, ".o");
		if (objdir == NULL) {
			ofiles[i] = strdup(filename);
		} else {
			snprintf(pathname, sizeof(pathname), "%s/%s", objdir, filename);
			ofiles[i] = strdup(pathname);
		}
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
