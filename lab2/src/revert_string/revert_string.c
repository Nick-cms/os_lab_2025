#include "revert_string.h"
#include <string.h>

void RevertString(char *str)
{
	int len = strlen(str);
	char *begin = str;
	char *end = str + len - 1;
	while (begin < end) {
		char tmp = *begin;
		*begin = *end;
		*end = tmp;
		begin++;
		end--;
	}
}

