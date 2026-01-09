-- embedded/adapter/diagnostics.lua
-- Adapter for script/core/diagnostics.lua

local common = require 'embedded.adapter.common'
local workspace_adapter = require 'embedded.adapter.workspace'

local adapter = {}

--- Get diagnostics for a file
---@param uri string
---@return table[] Array of diagnostics
function adapter.get_diagnostics(uri)
    local state = workspace_adapter.get_state(uri)
    if not state then
        return {}
    end
    
    local success, result = common.safe_call(function()
        local diagnostics = require 'core.diagnostics'
        
        -- Get diagnostics from LuaLS
        local diag_list = diagnostics(uri, state)
        
        if not diag_list then
            return {}
        end
        
        -- Simplify for C consumption
        local simplified = {}
        for _, diag in ipairs(diag_list) do
            table.insert(simplified, common.simplify_diagnostic(diag))
        end
        
        return simplified
    end)
    
    if success then
        return result
    else
        print('Diagnostics error:', result)
        return {}
    end
end

return adapter
