#include "stdafx.h"

#include "UnitTests.h"

void json_parser_ut_all();
void strutil_ut_all();

int run_unit_tests()
{
	json_parser_ut_all();
	strutil_ut_all();
	assert(0 == unitTestsFailed());
	return unitTestsFailed();
}

