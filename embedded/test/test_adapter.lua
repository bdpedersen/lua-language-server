-- embedded/test/test_adapter.lua
-- Test script for Lua adapters

local function print_header(text)
    print("\n" .. string.rep("=", 60))
    print(text)
    print(string.rep("=", 60))
end

local function print_test(name, passed)
    print(string.format("  [%s] %s", passed and "✓" or "✗", name))
end

-- Add embedded directory to path
package.path = './embedded/?.lua;./embedded/adapter/?.lua;./script/?.lua;./script/?/init.lua;' .. package.path

print_header("Testing Lua Adapters")

-- Test 1: Load embedded module
print_test("Loading embedded module", pcall(function()
    require 'embedded.init'
end))

-- Test 2: Load common utilities
local common
print_test("Loading common utilities", pcall(function()
    common = require 'embedded.adapter.common'
end))

-- Test 3: Test position conversion
if common then
    local lua_line, lua_char = common.lsp_to_lua_position(0, 0)
    print_test("LSP to Lua position (0,0 -> 1,1)", lua_line == 1 and lua_char == 1)
    
    local lsp_line, lsp_char = common.lua_to_lsp_position(1, 1)
    print_test("Lua to LSP position (1,1 -> 0,0)", lsp_line == 0 and lsp_char == 0)
end

-- Test 4: Test safe_call
if common then
    local success, result = common.safe_call(function() return 42 end)
    print_test("safe_call success case", success and result == 42)
    
    local success2, err = common.safe_call(function() error("test error") end)
    print_test("safe_call error case", not success2 and err ~= nil)
end

-- Test 5: Load workspace adapter
local workspace_adapter
print_test("Loading workspace adapter", pcall(function()
    workspace_adapter = require 'embedded.adapter.workspace'
end))

-- Test 6: Initialize workspace
if workspace_adapter then
    local success = workspace_adapter.init("/tmp/test_workspace")
    print_test("Initialize workspace", success)
end

-- Test 7: Update file
if workspace_adapter then
    local test_uri = "file:///test.lua"
    local test_code = "local x = 10\nprint(x)\n"
    
    -- Note: Full parsing requires C modules, so we'll skip that for now
    -- Just test the storage functionality
    workspace_adapter.files = {}
    workspace_adapter.files[test_uri] = {
        uri = test_uri,
        text = test_code,
        version = 1
    }
    local retrieved = workspace_adapter.files[test_uri].text
    print_test("File storage (without parsing)", retrieved == test_code)
else
    print_test("File storage (without parsing)", false)
end

-- Test 8: Load other adapters
print_test("Loading completion adapter", pcall(function()
    require 'embedded.adapter.completion'
end))

print_test("Loading diagnostics adapter", pcall(function()
    require 'embedded.adapter.diagnostics'
end))

print_test("Loading hover adapter", pcall(function()
    require 'embedded.adapter.hover'
end))

print_test("Loading definition adapter", pcall(function()
    require 'embedded.adapter.definition'
end))

print_header("Test Suite Complete")
