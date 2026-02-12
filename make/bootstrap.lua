-- Pre-pass: extract --resource-dir for root (used when building package.path)
local resourceDir
for i = 1, #arg do
    local v = arg[i]
    local val = v:match('^%-%-resource%-dir=(.+)$')
    if val then
        resourceDir = val
        break
    end
    if v == '--resource-dir' and arg[i + 1] then
        resourceDir = arg[i + 1]
        break
    end
end

local main, exec
local i = 1
while arg[i] do
    if     arg[i] == '-E' then
    elseif arg[i] == '-e' then
        i = i + 1
        local expr = assert(arg[i], "'-e' needs argument")
        assert(load(expr, "=(command line)"))()
        -- exit after the executing
        exec = true
    elseif not main and arg[i]:sub(1, 1) ~= '-' then
        main = i
    elseif arg[i] == '--' then
        break
    end
    i = i + 1
end

if exec and not main then
    return
end

if main then
    for i = -1, -999, -1 do
        if not arg[i] then
            for j = i + 1, -1 do
                arg[j - main + 1] = arg[j]
            end
            break
        end
    end
    for j = 1, #arg do
        arg[j - main] = arg[j]
    end
    for j = #arg - main + 1, #arg do
        arg[j] = nil
    end
end

local root
do
    if resourceDir and resourceDir ~= '' then
        local fs = require 'bee.filesystem'
        root = fs.absolute(fs.path(resourceDir)):string()
        if root == '' then
            root = '.'
        end
        root = root:gsub('[/\\]', package.config:sub(1, 1))
        if not main then
            arg[0] = root .. package.config:sub(1, 1) .. 'main.lua'
        end
    elseif main then
        local fs = require 'bee.filesystem'
        local mainPath = fs.path(arg[0])
        root = mainPath:parent_path():string()
        if root == '' then
            root = '.'
        end
        root = root:gsub('[/\\]', package.config:sub(1, 1))
    else
        local sep = package.config:sub(1, 1)
        if sep == '\\' then
            sep = '/\\'
        end
        local pattern = "[" .. sep .. "]+[^" .. sep .. "]+"
        root = package.cpath:match("([^;]+)" .. pattern .. pattern .. "$")
        arg[0] = root .. package.config:sub(1, 1) .. 'main.lua'
        root = root:gsub('[/\\]', package.config:sub(1, 1))
    end
end

package.path = table.concat({
    root .. "/script/?.lua",
    root .. "/script/?/init.lua",
}, ";"):gsub('/', package.config:sub(1, 1))

package.searchers[2] = function (name)
    local filename, err = package.searchpath(name, package.path)
    if not filename then
        return err
    end
    local f = io.open(filename)
    if not f then
        return 'cannot open file:' .. filename
    end
    local buf = f:read '*a'
    f:close()
    local relative = filename:sub(1, #root) == root and filename:sub(#root + 2) or filename
    local init, err = load(buf, '@' .. relative)
    if not init then
        return err
    end
    return init, filename
end

assert(loadfile(arg[0]))(table.unpack(arg))
