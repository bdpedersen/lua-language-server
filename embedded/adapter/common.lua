-- embedded/adapter/common.lua
-- Common utilities for adapters

local common = {}

--- Convert LSP position to LuaLS position
---@param line integer (0-indexed LSP)
---@param character integer (0-indexed LSP)
---@return integer line (1-indexed Lua)
---@return integer character (1-indexed Lua)
function common.lsp_to_lua_position(line, character)
    return line + 1, character + 1
end

--- Convert LuaLS position to LSP position
---@param line integer (1-indexed Lua)
---@param character integer (1-indexed Lua)
---@return integer line (0-indexed LSP)
---@return integer character (0-indexed LSP)
function common.lua_to_lsp_position(line, character)
    return line - 1, character - 1
end

--- Convert completion item to simple table
---@param item table LuaLS completion item
---@return table Simple completion item
function common.simplify_completion(item)
    return {
        label = item.label or '',
        kind = item.kind or 1,
        detail = item.detail or '',
        documentation = item.documentation and item.documentation.value or '',
        insertText = item.insertText or item.label or '',
        sortText = item.sortText or item.label or '',
    }
end

--- Convert diagnostic to simple table
---@param diagnostic table LuaLS diagnostic
---@return table Simple diagnostic
function common.simplify_diagnostic(diagnostic)
    return {
        line = diagnostic.range and diagnostic.range.start.line or 0,
        start_char = diagnostic.range and diagnostic.range.start.character or 0,
        end_char = diagnostic.range and diagnostic.range['end'].character or 0,
        severity = diagnostic.severity or 1,
        message = diagnostic.message or '',
        source = diagnostic.source or 'lua',
        code = diagnostic.code or '',
    }
end

--- Safely call a function and return result or error
---@param func function
---@param ... any Arguments
---@return boolean success
---@return any result or error message
function common.safe_call(func, ...)
    local success, result = pcall(func, ...)
    if success then
        return true, result
    else
        return false, tostring(result)
    end
end

return common
