-- embedded/adapter/definition.lua
-- Adapter for script/core/definition.lua

local common = require 'embedded.adapter.common'
local workspace_adapter = require 'embedded.adapter.workspace'

local adapter = {}

--- Get definition locations
---@param uri string
---@param line integer (0-indexed LSP)
---@param character integer (0-indexed LSP)
---@return table[] Array of locations
function adapter.get_definition(uri, line, character)
    local state = workspace_adapter.get_state(uri)
    if not state then
        return {}
    end
    
    local lua_line, lua_char = common.lsp_to_lua_position(line, character)
    
    local success, result = common.safe_call(function()
        local definition = require 'core.definition'
        
        -- Calculate offset
        local text = workspace_adapter.get_text(uri)
        if not text then
            return {}
        end
        
        local offset = 0
        local current_line = 1
        for i = 1, #text do
            if current_line == lua_line then
                offset = i + lua_char - 1
                break
            end
            if text:sub(i, i) == '\n' then
                current_line = current_line + 1
            end
        end
        
        -- Get definition
        local locations = definition(uri, offset, state)
        
        if not locations then
            return {}
        end
        
        -- Simplify
        local simplified = {}
        for _, loc in ipairs(locations) do
            table.insert(simplified, {
                uri = loc.uri,
                line = loc.range and loc.range.start.line or 0,
                character = loc.range and loc.range.start.character or 0,
            })
        end
        
        return simplified
    end)
    
    if success then
        return result
    else
        print('Definition error:', result)
        return {}
    end
end

return adapter
