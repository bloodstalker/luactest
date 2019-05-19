#include <stdio.h>
#include <inttypes.h>

#include "./lua/src/lauxlib.h"
#include "./lua/src/lua.h"
#include "./lua/src/lualib.h"
#include "./lua.c"

#include "test.h"

void reg_all(lua_State * ls)
{
	lua_register(ls, "myfunc1", myfunc1_lwrapper);
	lua_register(ls, "str2int", str2int_l);
}

#pragma weak main
int main (int argc, char** argv) {
  int status, result;
  lua_State *ls = luaL_newstate();
  if (ls == NULL) {
    l_message(argv[0], "cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  reg_all(ls);
  lua_pushcfunction(ls, &pmain);
  lua_pushinteger(ls, argc);
  lua_pushlightuserdata(ls, argv);
  status = lua_pcall(ls, 2, 1, 0);
  result = lua_toboolean(ls, -1);
  report(ls, status);
  lua_close(ls);
  return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
