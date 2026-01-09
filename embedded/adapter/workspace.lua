-- embedded/adapter/workspace.lua
-- Adapter for script/workspace/workspace.lua

local common = require 'embedded.adapter.common'

local adapter = {}
local workspace_instance = nil

--- Initialize workspace
---@param path string
---@return boolean success
function adapter.init(path)
    local success, result = common.safe_call(function()
        -- Create workspace instance
        -- Note: We're NOT using LuaLS's workspace module to avoid dependencies
        -- Instead, we create a minimal workspace structure for embedded mode
        workspace_instance = {
            path = path,
            files = {},
        }
        
        return true
    end)
    
    return success
end

--- Update file in workspace
---@param uri string
---@param text string
function adapter.update_file(uri, text)
    if not workspace_instance then
        return
    end
    
    -- Store in our cache
    workspace_instance.files[uri] = {
        uri = uri,
        text = text,
        version = (workspace_instance.files[uri] and 
                   workspace_instance.files[uri].version or 0) + 1,
    }
    
    -- Compile the file using LuaLS's parser
    local parser = require 'parser'
    local state = parser.compile(text, 'Lua', 'Lua 5.4')
    
    if state then
        workspace_instance.files[uri].state = state
    end
end

--- Get compiled state for a file
---@param uri string
---@return table|nil Compiled state
function adapter.get_state(uri)
    if not workspace_instance or not workspace_instance.files[uri] then
        return nil
    end
    
    return workspace_instance.files[uri].state
end

--- Get file text
---@param uri string
---@return string|nil
function adapter.get_text(uri)
    if not workspace_instance or not workspace_instance.files[uri] then
        return nil
    end
    
    return workspace_instance.files[uri].text
end

return adapter
