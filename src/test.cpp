#include "test.h"
#include "ChunkedInputStream.h"
#include "Dictionary.h"
#include "FileUtil.h"
#include "MemoryInputStream.h"
#include "NGWord.h"
#include "OAuth.h"
#include "ParsedUri.h"
#include "StringUtil.h"
#include "Twitter.h"
#include "acl.h"
#include "subr.h"

int test_count;
int test_fail;

void
xp_eq_(const char *file, int line, const char *func,
	int exp, int act, const std::string& msg)
{
	test_count++;

	if (exp != act) {
		test_fail++;
		printf("%s:%d: %s(%s) expects %d but %d\n",
			file, line, func, msg.c_str(), exp, act);
	}
}

void
xp_eq_(const char *file, int line, const char *func,
	const std::string& exp, const std::string& act, const std::string& msg)
{
	test_count++;

	if (exp != act) {
		test_fail++;
		printf("%s:%d: %s(%s) expects \"%s\" but \"%s\"\n",
			file, line, func, msg.c_str(), exp.c_str(), act.c_str());
	}
}

void
xp_fail_(const char *file, int line, const char *func,
	const std::string& msg)
{
	test_count++;
	test_fail++;
	printf("%s:%d: %s(%s) failed\n", file, line, func, msg.c_str());
}

int
main(int ac, char *av[])
{
	test_count = 0;
	test_fail = 0;

	test_ChunkedInputStream();
	test_Dictionary();
	test_FileUtil();
	test_MemoryInputStream();
	test_NGWord();
	test_OAuth();
	test_ParsedUri();
	test_StringUtil();
	test_Twitter();
	test_acl();
	test_subr();

	printf("%d tests", test_count);
	if (test_fail == 0) {
		printf(", all passed.\n");
	} else {
		printf(", %d faild!!\n", test_fail);
	}
		
	return 0;
}
