-- embedded/adapter/completion.lua
-- Adapter for script/core/completion.lua

local common = require 'embedded.adapter.common'
local workspace_adapter = require 'embedded.adapter.workspace'

local adapter = {}

--- Get completions at a position
---@param uri string
---@param line integer (0-indexed LSP)
---@param character integer (0-indexed LSP)
---@return table[] Array of completion items
function adapter.get_completions(uri, line, character)
    -- Get compiled state
    local state = workspace_adapter.get_state(uri)
    if not state then
        return {}
    end
    
    -- Convert to Lua positions (1-indexed)
    local lua_line, lua_char = common.lsp_to_lua_position(line, character)
    
    -- Call LuaLS completion engine
    local success, result = common.safe_call(function()
        local completion = require 'core.completion'
        
        -- Get position offset in the text
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
        
        -- Request completions
        local items = completion.completion(uri, offset, state)
        
        if not items then
            return {}
        end
        
        -- Simplify for C consumption
        local simplified = {}
        for _, item in ipairs(items) do
            table.insert(simplified, common.simplify_completion(item))
        end
        
        return simplified
    end)
    
    if success then
        return result
    else
        print('Completion error:', result)
        return {}
    end
end

return adapter
