
#include <stdio.h>
#include "./lua/src/lauxlib.h"
#include "./lua/src/lua.h"
#include "./lua/src/lualib.h"

void myfunc1(void)
{
	printf("die die die\n");
}

int myfunc1_lwrapper(lua_State* ls)
{
	myfunc1();
}

int str2int(const char* str)
{
	int res = 0;
	int dec = 10;
	for (int i = 0;; ++i) {
		if (str[i] == '\0') break;

		if ('0' == str[i]) {
			res = res*dec + 0;
		}
		else if ('1' == str[i]) {
			res = res*dec + 1;
		}
		else if ('2' == str[i]) {
			res = res*dec + 2;
		}
		else if ('3' == str[i]) {
			res = res*dec + 3;
		}
		else if ('4' == str[i]) {
			res = res*dec + 4;
		}
		else if ('5' == str[i]) {
			res = res*dec + 5;
		}
		else if ('6' == str[i]) {
			res = res*dec + 6;
		}
		else if ('7' == str[i]) {
			res = res*dec + 7;
		}
		else if ('8' == str[i]) {
			res = res*dec + 8;
		}
		else if ('9' == str[i]) {
			res = res*dec + 9;
		}
		else {}
	}

	return res;
}

int str2int_l(lua_State *ls)
{
	const char* str = lua_tostring(ls, 1);
	int res = str2int(str);
	lua_pushinteger(ls, res);
	return 1;
}
