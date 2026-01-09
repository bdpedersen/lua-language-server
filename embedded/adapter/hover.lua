-- embedded/adapter/hover.lua
-- Adapter for script/core/hover.lua

local common = require 'embedded.adapter.common'
local workspace_adapter = require 'embedded.adapter.workspace'

local adapter = {}

--- Get hover information at position
---@param uri string
---@param line integer (0-indexed LSP)
---@param character integer (0-indexed LSP)
---@return table|nil Hover info or nil
function adapter.get_hover(uri, line, character)
    local state = workspace_adapter.get_state(uri)
    if not state then
        return nil
    end
    
    local lua_line, lua_char = common.lsp_to_lua_position(line, character)
    
    local success, result = common.safe_call(function()
        local hover = require 'core.hover'
        
        -- Calculate offset
        local text = workspace_adapter.get_text(uri)
        if not text then
            return nil
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
        
        -- Get hover info
        local hover_result = hover.getHover(uri, offset, state)
        
        if not hover_result then
            return nil
        end
        
        return {
            content = hover_result.label or hover_result.description or '',
            markdown = hover_result.markdown or false,
        }
    end)
    
    if success then
        return result
    else
        print('Hover error:', result)
        return nil
    end
end

return adapter
