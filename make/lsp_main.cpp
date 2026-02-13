/**
 * liblualsp: LSPInit, LSPWriteInput, LSPDisconnect, LSPMain
 * Used by lua-language-server-q to run the LSP in queue/embedding mode.
 */

#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include <lua.hpp>
#include <string_view>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <atomic>

#include <bee/lua/module.h>
#include "3rd/lua-patch/bee_lua55.h"
#include "3rd/lua-patch/bee_newstate.h"

// Forward declare transport C API
extern "C" {
void transport_register_queue(void (*output_cb)(const char* data, size_t len));
void LSPWriteInput(const char* data, size_t len);
void LSPDisconnect(void);
}

static int l_message(const char* pname, const char* msg) {
    if (pname) fprintf(stderr, "%s: ", pname);
    fprintf(stderr, "%s\n", msg);
    return 0;
}

static int report(lua_State* L, int status) {
    if (status != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        l_message("lua-language-server", msg ? msg : "unknown error");
        lua_pop(L, 1);
    }
    return status;
}

static int msghandler(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (msg == NULL) {
        if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING)
            return 1;
        else
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);
    return 1;
}

static int docall(lua_State* L, int narg, int nres) {
    int base = lua_gettop(L) - narg;
    lua_pushcfunction(L, msghandler);
    lua_insert(L, base);
    int status = lua_pcall(L, narg, nres, base);
    lua_remove(L, base);
    return status;
}

static void createargtable(lua_State* L, char** argv, int argc) {
    lua_createtable(L, argc - 1, 2);
    lua_pushstring(L, argv[0] ? argv[0] : "lua-language-server-q");
    lua_rawseti(L, -2, -1);
    lua_pushstring(L, "!main.lua");
    lua_rawseti(L, -2, 0);
    for (int i = 1; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "arg");
}

/** When os.exit(code, true) is called in inprocess mode, set flag and return (event loop will break). */
static int lsp_exit_patch(lua_State* L) {
    (void)luaL_optinteger(L, 1, 0);
    int close = lua_toboolean(L, 2);
    if (close) {
        lua_pushboolean(L, 1);
        lua_setglobal(L, "LSP_SHUTDOWN");
        return 0;
    }
    return 0;
}

static void patch_os_exit(lua_State* L) {
    lua_getglobal(L, "os");
    if (lua_istable(L, -1)) {
        lua_pushcfunction(L, lsp_exit_patch);
        lua_setfield(L, -2, "exit");
    }
    lua_pop(L, 1);
}

static int pushargs(lua_State* L) {
    if (lua_getglobal(L, "arg") != LUA_TTABLE)
        luaL_error(L, "'arg' is not a table");
    int n = (int)luaL_len(L, -1);
    luaL_checkstack(L, n + 3, "too many arguments");
    for (int i = 1; i <= n; i++)
        lua_rawgeti(L, -i, i);
    lua_remove(L, -n - 1);
    return n;
}

static constexpr std::string_view bootstrap = R"BOOTSTRAP(
    INPROCESS = true
    LSP_SHUTDOWN = false
    local function loadfile(filename)
        local file  = assert(io.open(filename))
        local str  = file:read "a"
        file:close()
        return load(str, "=(main.lua)")
    end
    local sys = require "bee.sys"
    local platform = require "bee.platform"
    local progdir = sys.exe_path():parent_path()
    local mainlua = (progdir / "main.lua"):string()
    if platform.os == "windows" then
        package.cpath = (progdir / "?.dll"):string()
    else
        package.cpath = (progdir / "?.so"):string()
    end
    assert(loadfile(mainlua))(...)
)BOOTSTRAP";

static int run_bootstrap(lua_State* L) {
    int status = luaL_loadbuffer(L, bootstrap.data(), bootstrap.size(), "=(bootstrap.lua)");
    if (status == LUA_OK) {
        int n = pushargs(L);
        status = docall(L, n, 0);
    }
    return report(L, status);
}

static int pmain_lsp(lua_State* L) {
    int argc = (int)lua_tointeger(L, 1);
    char** argv = (char**)lua_touserdata(L, 2);
    luaL_checkversion(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
    luaL_openlibs(L);
    createargtable(L, argv, argc);
    patch_os_exit(L);  /* os.exit(code, true) -> error(LSP_SHUTDOWN) so LSPMain returns */
    lua_gc(L, LUA_GCGEN, 0, 0);
    if (run_bootstrap(L) != LUA_OK)
        return 0;
    lua_pushboolean(L, 1);
    return 1;
}

static char** g_argv = nullptr;
static int g_argc = 0;
static void (*g_output_cb)(const char*, size_t) = nullptr;

extern "C" {

void LSPInit(int argc, char** argv, void (*output_cb)(const char* data, size_t len)) {
    g_argc = argc;
    g_argv = argv;
    g_output_cb = output_cb;
    transport_register_queue(output_cb);
}

void LSPMain(void) {
    if (!g_argv || !g_output_cb) {
        return;
    }
    lua_State* L = bee_lua_newstate();
    if (!L) {
        l_message("lua-language-server-q", "cannot create state: not enough memory");
        return;
    }
    lua_pushcfunction(L, pmain_lsp);
    lua_pushinteger(L, g_argc);
    lua_pushlightuserdata(L, g_argv);
    int status = lua_pcall(L, 2, 1, 0);
    int result = lua_toboolean(L, -1);
    report(L, status);
    lua_close(L);
    (void)result;
}

} // extern "C"
