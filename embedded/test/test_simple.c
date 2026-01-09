// embedded/test/test_simple.c
// Minimal test to check Lua state creation

#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// Manually open standard libraries since bee.lua uses custom openlibs
static void openlibs(lua_State* L) {
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "package", luaopen_package, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "coroutine", luaopen_coroutine, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "io", luaopen_io, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "os", luaopen_os, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "string", luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "math", luaopen_math, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "utf8", luaopen_utf8, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "debug", luaopen_debug, 1);
    lua_pop(L, 1);
}

int main() {
    printf("Creating Lua state...\n");
    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Failed to create Lua state\n");
        return 1;
    }
    printf("Lua state created successfully\n");
    
    printf("Opening libraries...\n");
    openlibs(L);
    printf("Libraries opened\n");
    
    printf("Running simple code...\n");
    int result = luaL_dostring(L, "print('Hello from Lua!')");
    if (result != LUA_OK) {
        fprintf(stderr, "Error: %s\n", lua_tostring(L, -1));
        return 1;
    }
    
    printf("Closing Lua state...\n");
    lua_close(L);
    printf("Done!\n");
    
    return 0;
}
