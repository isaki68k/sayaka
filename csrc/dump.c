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
#include <stdio.h>
#include <string.h>

diag dummy;

static string *
string_fgets(FILE *fp)
{
	char buf[1024];

	string *s = string_init();
	while (fgets(buf, sizeof(buf), fp)) {
		string_append_cstr(s, buf);

		int n = strlen(buf);
		if (n > 0 && buf[n - 1] == '\n') {
			break;
		}
	}
	if (string_len(s) == 0) {
		string_free(s);
		s = NULL;
	}
	return s;
}

static void
dump(FILE *fp)
{
	string *line;

	while ((line = string_fgets(fp)) != NULL) {
		json *js = json_create(&dummy);
		json_parse(js, line);
		json_dump(js, 0);
		string_free(line);
	}
}

int
main(int ac, char *av[])
{
	FILE *fp;

	if (ac > 1) {
		fp = fopen(av[1], "r");
		dump(fp);
		fclose(fp);
	} else {
		dump(stdin);
	}
	return 0;
}
