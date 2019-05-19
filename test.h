
#include "./lua/src/lauxlib.h"
#include "./lua/src/lua.h"
#include "./lua/src/lualib.h"

void myfunc1(void);
int myfunc1_lwrapper(lua_State* ls);
int str2int(const char* str);
int str2int_l(lua_State *ls);
