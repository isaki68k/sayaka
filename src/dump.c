/* vi:set ts=4: */
/*
 * Copyright (C) 2024 Tetsuya Isaki
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

//
// JSON ダンプ
//

#include "sayaka.h"
#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

static struct diag dummy;
static bool opt_j;

static void
dump(FILE *fp)
{
	string *line;

	while ((line = string_fgets(fp)) != NULL) {
		struct json *js = json_create(&dummy);
		json_parse(js, line);
		if (opt_j) {
			json_jsmndump(js);
		} else {
			json_dump(js, 0);
		}
		json_destroy(js);
		string_free(line);
	}
}

static void
usage(void)
{
	errx(1, "usage: %s [-j] <-|jsonfiles...>", getprogname());
}

int
main(int ac, char *av[])
{
	FILE *fp;
	int c;

	while ((c = getopt(ac, av, "j")) != -1) {
		switch (c) {
		 case 'j':
			opt_j = true;
			break;
		 default:
			usage();
			break;
		}
	}
	ac -= optind;
	av += optind;

	if (ac == 0) {
		usage();
	}
	for (int i = 0; i < ac; i++) {
		if (strcmp(av[i], "-") == 0) {
			dump(stdin);
		} else {
			fp = fopen(av[i], "r");
			dump(fp);
			fclose(fp);
		}
	}
	return 0;
}
