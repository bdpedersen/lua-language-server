---@diagnostic disable: lowercase-global

-- embedded/init.lua
-- Initialize embedded library mode for LuaLS

local embedded = {}

-- Add script directory to package path
local script_path = arg and arg[0] and arg[0]:match("(.*/)")
if script_path then
    package.path = script_path .. '../script/?.lua;' .. package.path
end

-- Disable features not needed in embedded mode
_G.EMBEDDED_MODE = true
_G.NO_NETWORK = true      -- Disable update checks
_G.NO_FILE_WATCHING = true -- Disable file watchers (Swift handles this)

-- Load adapters
embedded.workspace   = require 'embedded.adapter.workspace'
embedded.completion  = require 'embedded.adapter.completion'
embedded.diagnostics = require 'embedded.adapter.diagnostics'
embedded.hover       = require 'embedded.adapter.hover'
embedded.definition  = require 'embedded.adapter.definition'
embedded.common      = require 'embedded.adapter.common'

-- Global state
embedded.state = {
    initialized = false,
    workspace_path = nil,
    files = {},  -- URI -> content mapping
}

--- Initialize the embedded LSP engine
---@param workspace_path string
---@return boolean success
function embedded.initialize(workspace_path)
    if embedded.state.initialized then
        return true
    end
    
    embedded.state.workspace_path = workspace_path
    
    -- Initialize workspace
    local success = embedded.workspace.init(workspace_path)
    if not success then
        return false
    end
    
    embedded.state.initialized = true
    return true
end

--- Update file content
---@param uri string
---@param text string
function embedded.set_file_content(uri, text)
    embedded.state.files[uri] = text
    embedded.workspace.update_file(uri, text)
end

--- Get file content
---@param uri string
---@return string|nil
function embedded.get_file_content(uri)
    return embedded.state.files[uri]
end

return embedded
