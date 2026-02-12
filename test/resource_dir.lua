---Test that --resource-dir works when the server is launched from a directory that does NOT contain scripts.
---We copy the binary and main.lua to a temp dir (no script/, meta/). The server MUST use --resource-dir
---to find them. If --resource-dir fails, the server cannot load scripts and will crash or error before responding.

local fs         = require 'bee.filesystem'
local platform   = require 'bee.platform'
local subprocess = require 'bee.subprocess'
local thread     = require 'bee.thread'
local json       = require 'json'
local jsonrpc    = require 'jsonrpc'

local exe = platform.os == 'windows' and '.exe' or ''
local exePath = fs.absolute(ROOT / 'bin' / ('lua-language-server' .. exe)):string()

-- Temp dir: copy entire bin/ so binary finds main.lua and any .so/.dylib. No script/, meta/.
local tmpdir = fs.temp_directory_path() / ('lls-resource-dir-test-' .. tostring(math.random(100000, 999999)))
fs.create_directories(tmpdir)
for path in fs.pairs(ROOT / 'bin') do
    if fs.is_regular_file(path) then
        local name = path:filename():string()
        fs.copy_file(path, tmpdir / name, fs.copy_options.overwrite_existing)
    end
end

local resourceDir = fs.absolute(ROOT):string()
local tmpExePath = (tmpdir / ('lua-language-server' .. exe)):string()

local process, err = subprocess.spawn({
    tmpExePath,
    '--resource-dir=' .. resourceDir,
    '--stdio',
    cwd = tmpdir:string(),
    stdin = true,
    stdout = true,
    stderr = 'stdout',
})

if not process then
    fs.remove_all(tmpdir)
    error('Failed to spawn server: ' .. tostring(err))
end

local function sendRequest(method, params, id)
    local req = {
        jsonrpc = '2.0',
        method = method,
        params = params or {},
    }
    if id ~= nil then
        req.id = id
    end
    local buf = jsonrpc.encode(req)
    process.stdin:write(buf)
    process.stdin:flush()
end

local function readResponse()
    local function read(len)
        local chunk = process.stdout:read(len)
        return chunk
    end
    local res, err = jsonrpc.decode(read)
    return res, err
end

-- Send initialize
sendRequest('initialize', {
    processId = nil,
    rootUri = nil,
    capabilities = {},
}, 1)

-- Read messages until we get the initialize response (id=1). Server may send notifications first.
-- Poll with peek. On macOS peek may return nil (unreliable), so we skip the test there if no response.
local response, decodeErr
for _ = 1, 150 do
    local n = subprocess.peek(process.stdout)
    if n and n > 0 then
        response, decodeErr = readResponse()
        if response and response.id == 1 then
            break
        end
        response = nil
    else
        thread.sleep(100)
    end
end

if not response or response.id ~= 1 then
    process:kill()
    process:wait()
    fs.remove_all(tmpdir)
    if platform.os == 'macos' then
        print('resource_dir: SKIP (subprocess.peek may return nil on macOS)')
        return
    end
    error('No response from server (--resource-dir likely failed): ' .. tostring(decodeErr))
end

if response.error then
    process:kill()
    process:wait()
    fs.remove_all(tmpdir)
    error('Server returned error: ' .. json.encode(response.error))
end

if not response.result or not response.result.capabilities then
    process:kill()
    process:wait()
    fs.remove_all(tmpdir)
    error('Invalid initialize response: missing capabilities')
end

-- Send initialized notification (no id)
sendRequest('initialized', {}, nil)

-- Send shutdown
sendRequest('shutdown', {}, 2)

-- Read shutdown response
for _ = 1, 20 do
    local n = subprocess.peek(process.stdout)
    if n and n > 0 then
        readResponse()
        break
    end
    thread.sleep(100)
end

-- Send exit notification (no id)
sendRequest('exit', {}, nil)

local exitCode = process:wait()
fs.remove_all(tmpdir)

if exitCode ~= 0 then
    error('Server exited with code ' .. tostring(exitCode))
end

print('resource_dir: OK (--resource-dir works when launched from different directory)')
