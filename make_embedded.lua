-- make_embedded.lua
-- Build configuration for embedded LuaLS library

local lm = require 'luamake'

-- Import main configuration
lm.c = lm.compiler == 'msvc' and 'c89' or 'c11'
lm.cxx = 'c++17'
lm.lua = '55'

require "make.detect_platform"

-- Import bee.lua for Lua source set
lm:import "3rd/bee.lua/make.lua"

-- LPegLabel dependency (required by parser)
lm:source_set 'lpeglabel' {
    rootdir = '3rd',
    includes = "bee.lua/3rd/lua55",
    sources = "lpeglabel/*.c",
    defines = {
        'MAXRECLEVEL=1000',
    },
    deps = "source_lua",
}

-- Embedded library target
lm:shared_library 'libluals_embedded' {
    deps = {
        "source_lua",
        "lpeglabel",
    },
    includes = {
        "3rd/bee.lua/3rd/lua55",
        "embedded/bridge",
    },
    sources = {
        "embedded/bridge/libluals.c",
    },
    defines = {
        "LUALS_EMBEDDED_BUILD",
    },
    visibility = "default",
    macos = {
        flags = "-fPIC",
        defines = "LUA_USE_MACOSX",
        links = { "m", "dl" },
    },
    linux = {
        crt = "static",
        flags = "-fPIC",
        defines = "LUA_USE_LINUX",
        links = { "m", "dl" },
    },
    ios = {
        flags = "-fPIC",
        defines = "LUA_USE_IOS",
    },
}

-- Copy Lua scripts to bin/embedded
lm:copy 'copy_embedded_init' {
    inputs = "embedded/init.lua",
    outputs = "bin/embedded/init.lua",
}

lm:copy 'copy_embedded_common' {
    inputs = "embedded/adapter/common.lua",
    outputs = "bin/embedded/adapter/common.lua",
}

lm:copy 'copy_embedded_workspace' {
    inputs = "embedded/adapter/workspace.lua",
    outputs = "bin/embedded/adapter/workspace.lua",
}

lm:copy 'copy_embedded_completion' {
    inputs = "embedded/adapter/completion.lua",
    outputs = "bin/embedded/adapter/completion.lua",
}

lm:copy 'copy_embedded_diagnostics' {
    inputs = "embedded/adapter/diagnostics.lua",
    outputs = "bin/embedded/adapter/diagnostics.lua",
}

lm:copy 'copy_embedded_hover' {
    inputs = "embedded/adapter/hover.lua",
    outputs = "bin/embedded/adapter/hover.lua",
}

lm:copy 'copy_embedded_definition' {
    inputs = "embedded/adapter/definition.lua",
    outputs = "bin/embedded/adapter/definition.lua",
}

-- Test executable for C API
lm:executable 'test_c_api' {
    deps = {
        "libluals_embedded",
        "source_lua",
    },
    includes = {
        "3rd/bee.lua/3rd/lua55",
        "embedded/bridge",
    },
    sources = {
        "embedded/test/test_c_api.c",
    },
    macos = {
        links = { "m", "dl" },
        ldflags = { "-Lbuild/bin", "-lluals_embedded" },
    },
    linux = {
        links = { "m", "dl" },
        ldflags = { "-Lbuild/bin", "-lluals_embedded" },
    },
}

-- Simple test to verify Lua works
lm:executable 'test_simple' {
    deps = "source_lua",
    includes = {
        "3rd/bee.lua/3rd/lua55",
    },
    sources = {
        "embedded/test/test_simple.c",
    },
    macos = {
        links = { "m", "dl" },
        defines = "LUA_USE_MACOSX",
    },
    linux = {
        links = { "m", "dl" },
        defines = "LUA_USE_LINUX",
    },
}

-- Main target for embedded build
lm:phony "embedded" {
    deps = {
        "libluals_embedded",
        "copy_embedded_init",
        "copy_embedded_common",
        "copy_embedded_workspace",
        "copy_embedded_completion",
        "copy_embedded_diagnostics",
        "copy_embedded_hover",
        "copy_embedded_definition",
    },
}

-- Test target
lm:phony "test_embedded" {
    deps = {
        "embedded",
        "test_c_api",
    },
}

lm:default {
    "embedded",
}
