// embedded/bridge/libluals.c
#include "libluals.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

struct luals_engine {
    lua_State* L;
    int embedded_ref;  // Reference to embedded module
};

// Helper: Get string field from Lua table
static char* get_string_field(lua_State* L, int index, const char* field) {
    lua_getfield(L, index, field);
    const char* str = lua_tostring(L, -1);
    char* result = str ? strdup(str) : NULL;
    lua_pop(L, 1);
    return result;
}

// Helper: Get integer field from Lua table
static int get_int_field(lua_State* L, int index, const char* field) {
    lua_getfield(L, index, field);
    int value = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return value;
}

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

luals_engine_t* luals_engine_create(const char* workspace_path) {
    luals_engine_t* engine = malloc(sizeof(luals_engine_t));
    if (!engine) return NULL;
    
    // Create Lua state
    engine->L = luaL_newstate();
    if (!engine->L) {
        free(engine);
        return NULL;
    }
    
    openlibs(engine->L);
    
    // Set package path to include LuaLS directories
    lua_getglobal(engine->L, "package");
    lua_getfield(engine->L, -1, "path");
    const char* current_path = lua_tostring(engine->L, -1);
    
    char new_path[2048];
    snprintf(new_path, sizeof(new_path),
             "%s;./bin/embedded/?.lua;./bin/embedded/adapter/?.lua;./script/?.lua;./script/?/init.lua",
             current_path);
    
    lua_pop(engine->L, 1);
    lua_pushstring(engine->L, new_path);
    lua_setfield(engine->L, -2, "path");
    lua_pop(engine->L, 1);
    
    // Load embedded module
    if (luaL_dofile(engine->L, "bin/embedded/init.lua") != LUA_OK) {
        fprintf(stderr, "Failed to load bin/embedded/init.lua: %s\n",
                lua_tostring(engine->L, -1));
        lua_close(engine->L);
        free(engine);
        return NULL;
    }
    
    // Store reference to embedded module
    engine->embedded_ref = luaL_ref(engine->L, LUA_REGISTRYINDEX);
    
    // Initialize workspace
    lua_rawgeti(engine->L, LUA_REGISTRYINDEX, engine->embedded_ref);
    lua_getfield(engine->L, -1, "initialize");
    lua_pushvalue(engine->L, -2);  // self
    lua_pushstring(engine->L, workspace_path);
    
    if (lua_pcall(engine->L, 2, 1, 0) != LUA_OK) {
        fprintf(stderr, "Failed to initialize: %s\n", lua_tostring(engine->L, -1));
        lua_close(engine->L);
        free(engine);
        return NULL;
    }
    
    bool success = lua_toboolean(engine->L, -1);
    lua_pop(engine->L, 2);  // Pop result and embedded module
    
    if (!success) {
        lua_close(engine->L);
        free(engine);
        return NULL;
    }
    
    return engine;
}

void luals_set_file_content(luals_engine_t* engine, const char* uri, const char* text) {
    lua_State* L = engine->L;
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, engine->embedded_ref);
    lua_getfield(L, -1, "set_file_content");
    lua_pushvalue(L, -2);  // self
    lua_pushstring(L, uri);
    lua_pushstring(L, text);
    
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        fprintf(stderr, "Error setting file content: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    
    lua_pop(L, 1);  // Pop embedded module
}

luals_completion_t* luals_get_completions(
    luals_engine_t* engine,
    const char* uri,
    int line,
    int character,
    int* out_count
) {
    lua_State* L = engine->L;
    
    // Get embedded.completion.get_completions
    lua_rawgeti(L, LUA_REGISTRYINDEX, engine->embedded_ref);
    lua_getfield(L, -1, "completion");
    lua_getfield(L, -1, "get_completions");
    
    // Push arguments
    lua_pushstring(L, uri);
    lua_pushinteger(L, line);
    lua_pushinteger(L, character);
    
    // Call function
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        fprintf(stderr, "Completion error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    // Parse result table
    if (!lua_istable(L, -1)) {
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    int count = (int)lua_rawlen(L, -1);
    if (count == 0) {
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    luals_completion_t* completions = malloc(sizeof(luals_completion_t) * count);
    
    for (int i = 0; i < count; i++) {
        lua_rawgeti(L, -1, i + 1);
        
        completions[i].label = get_string_field(L, -1, "label");
        completions[i].kind = get_int_field(L, -1, "kind");
        completions[i].detail = get_string_field(L, -1, "detail");
        completions[i].documentation = get_string_field(L, -1, "documentation");
        completions[i].insert_text = get_string_field(L, -1, "insertText");
        
        lua_pop(L, 1);
    }
    
    lua_pop(L, 3);  // Pop result, completion module, embedded module
    *out_count = count;
    return completions;
}

luals_diagnostic_t* luals_get_diagnostics(
    luals_engine_t* engine,
    const char* uri,
    int* out_count
) {
    lua_State* L = engine->L;
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, engine->embedded_ref);
    lua_getfield(L, -1, "diagnostics");
    lua_getfield(L, -1, "get_diagnostics");
    lua_pushstring(L, uri);
    
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        fprintf(stderr, "Diagnostics error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    if (!lua_istable(L, -1)) {
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    int count = (int)lua_rawlen(L, -1);
    if (count == 0) {
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    luals_diagnostic_t* diagnostics = malloc(sizeof(luals_diagnostic_t) * count);
    
    for (int i = 0; i < count; i++) {
        lua_rawgeti(L, -1, i + 1);
        
        diagnostics[i].line = get_int_field(L, -1, "line");
        diagnostics[i].start_char = get_int_field(L, -1, "start_char");
        diagnostics[i].end_char = get_int_field(L, -1, "end_char");
        diagnostics[i].severity = get_int_field(L, -1, "severity");
        diagnostics[i].message = get_string_field(L, -1, "message");
        diagnostics[i].source = get_string_field(L, -1, "source");
        
        lua_pop(L, 1);
    }
    
    lua_pop(L, 3);
    *out_count = count;
    return diagnostics;
}

luals_hover_t* luals_get_hover(
    luals_engine_t* engine,
    const char* uri,
    int line,
    int character
) {
    lua_State* L = engine->L;
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, engine->embedded_ref);
    lua_getfield(L, -1, "hover");
    lua_getfield(L, -1, "get_hover");
    lua_pushstring(L, uri);
    lua_pushinteger(L, line);
    lua_pushinteger(L, character);
    
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        fprintf(stderr, "Hover error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 3);
        return NULL;
    }
    
    if (lua_isnil(L, -1)) {
        lua_pop(L, 3);
        return NULL;
    }
    
    luals_hover_t* hover = malloc(sizeof(luals_hover_t));
    hover->content = get_string_field(L, -1, "content");
    
    lua_getfield(L, -1, "markdown");
    hover->is_markdown = lua_toboolean(L, -1);
    lua_pop(L, 1);
    
    lua_pop(L, 3);
    return hover;
}

luals_location_t* luals_get_definition(
    luals_engine_t* engine,
    const char* uri,
    int line,
    int character,
    int* out_count
) {
    lua_State* L = engine->L;
    
    lua_rawgeti(L, LUA_REGISTRYINDEX, engine->embedded_ref);
    lua_getfield(L, -1, "definition");
    lua_getfield(L, -1, "get_definition");
    lua_pushstring(L, uri);
    lua_pushinteger(L, line);
    lua_pushinteger(L, character);
    
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        fprintf(stderr, "Definition error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    if (!lua_istable(L, -1)) {
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    int count = (int)lua_rawlen(L, -1);
    if (count == 0) {
        lua_pop(L, 3);
        *out_count = 0;
        return NULL;
    }
    
    luals_location_t* locations = malloc(sizeof(luals_location_t) * count);
    
    for (int i = 0; i < count; i++) {
        lua_rawgeti(L, -1, i + 1);
        
        locations[i].uri = get_string_field(L, -1, "uri");
        locations[i].line = get_int_field(L, -1, "line");
        locations[i].character = get_int_field(L, -1, "character");
        
        lua_pop(L, 1);
    }
    
    lua_pop(L, 3);
    *out_count = count;
    return locations;
}

// Memory management functions
void luals_completions_free(luals_completion_t* completions, int count) {
    for (int i = 0; i < count; i++) {
        free(completions[i].label);
        free(completions[i].detail);
        free(completions[i].documentation);
        free(completions[i].insert_text);
    }
    free(completions);
}

void luals_diagnostics_free(luals_diagnostic_t* diagnostics, int count) {
    for (int i = 0; i < count; i++) {
        free(diagnostics[i].message);
        free(diagnostics[i].source);
    }
    free(diagnostics);
}

void luals_hover_free(luals_hover_t* hover) {
    if (hover) {
        free(hover->content);
        free(hover);
    }
}

void luals_locations_free(luals_location_t* locations, int count) {
    for (int i = 0; i < count; i++) {
        free(locations[i].uri);
    }
    free(locations);
}

void luals_engine_destroy(luals_engine_t* engine) {
    if (engine) {
        if (engine->L) {
            lua_close(engine->L);
        }
        free(engine);
    }
}
