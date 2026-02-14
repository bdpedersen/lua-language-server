/**
 * Transport C module for Lua LSP.
 * Provides stdio and queue backends for read/write.
 * Used by both lua-language-server (stdio) and lua-language-server-q (queue via liblualsp).
 */

#include <lua.hpp>
#include <cstdio>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <cstring>
#include <thread>
#include <atomic>

#if defined(_WIN32)
#    include <io.h>
#    define LSP_READ(fd, buf, len) _read((fd), (buf), (unsigned)(len))
#else
#    include <unistd.h>
#    define LSP_READ(fd, buf, len) read((fd), (buf), (len))
#endif

namespace transport {

enum class Backend { None, Stdio, Queue };

struct QueueState {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<char> buffer;
    std::atomic<bool> disconnected{false};
    void (*output_cb)(const char* data, size_t len) = nullptr;
};

static Backend g_backend = Backend::None;
static QueueState* g_queue = nullptr;

// Stdio backend
static int stdio_read(lua_State* L) {
    lua_Integer len = luaL_checkinteger(L, 1);
    if (len <= 0 || len > 1024 * 1024) {
        return luaL_error(L, "invalid read length");
    }
    std::vector<char> buf(static_cast<size_t>(len));
    ssize_t n = LSP_READ(0, buf.data(), static_cast<size_t>(len));
    if (n <= 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, buf.data(), static_cast<size_t>(n));
    return 1;
}

static int stdio_write(lua_State* L) {
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    if (len > 0) {
        fwrite(data, 1, len, stdout);
        fflush(stdout);
    }
    return 0;
}

// Queue backend
static int queue_read(lua_State* L) {
    lua_Integer len = luaL_checkinteger(L, 1);
    if (len <= 0 || len > 1024 * 1024) {
        return luaL_error(L, "invalid read length");
    }
    QueueState* q = g_queue;
    if (!q) {
        lua_pushnil(L);
        return 1;
    }
    std::unique_lock<std::mutex> lock(q->mutex);
    q->cv.wait(lock, [q, len] {
        return q->disconnected.load() || static_cast<lua_Integer>(q->buffer.size()) >= len;
    });
    if (q->disconnected.load() && q->buffer.size() < static_cast<size_t>(len)) {
        lua_pushnil(L);
        return 1;
    }
    std::string result;
    result.reserve(static_cast<size_t>(len));
    for (lua_Integer i = 0; i < len && !q->buffer.empty(); ++i) {
        result.push_back(q->buffer.front());
        q->buffer.pop_front();
    }
    lock.unlock();
    lua_pushlstring(L, result.data(), result.size());
    return 1;
}

static int queue_write(lua_State* L) {
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    QueueState* q = g_queue;
    if (q && q->output_cb && len > 0) {
        q->output_cb(data, len);
    }
    return 0;
}

// Lua bindings
static int transport_read(lua_State* L) {
    switch (g_backend) {
    case Backend::Stdio:
        return stdio_read(L);
    case Backend::Queue:
        return queue_read(L);
    default:
        return luaL_error(L, "transport not registered");
    }
}

static int transport_write(lua_State* L) {
    switch (g_backend) {
    case Backend::Stdio:
        return stdio_write(L);
    case Backend::Queue:
        return queue_write(L);
    default:
        return luaL_error(L, "transport not registered");
    }
}

static int transport_register_stdio_lua(lua_State* L) {
    (void)L;
    g_backend = Backend::Stdio;
    g_queue = nullptr;
    return 0;
}

} // namespace transport

// C API for liblualsp
extern "C" {

void transport_register_stdio(void) {
    transport::g_backend = transport::Backend::Stdio;
    transport::g_queue = nullptr;
    setvbuf(stdout, nullptr, _IONBF, 0);
}

void transport_register_queue(void (*output_cb)(const char* data, size_t len)) {
    if (!transport::g_queue) {
        transport::g_queue = new transport::QueueState();
    }
    transport::g_queue->output_cb = output_cb;
    transport::g_queue->disconnected.store(false);
    transport::g_backend = transport::Backend::Queue;
}

void LSPWriteInput(const char* data, size_t len) {
    transport::QueueState* q = transport::g_queue;
    if (!q || !data) return;
    std::lock_guard<std::mutex> lock(q->mutex);
    for (size_t i = 0; i < len; ++i) {
        q->buffer.push_back(data[i]);
    }
    q->cv.notify_one();
}

void LSPDisconnect(void) {
    transport::QueueState* q = transport::g_queue;
    if (q) {
        q->disconnected.store(true);
        q->cv.notify_all();
    }
}

} // extern "C"

// Lua module
#include <bee/lua/module.h>

static int transport_disconnect_lua(lua_State* L) {
    (void)L;
    LSPDisconnect();
    return 0;
}

extern "C" int luaopen_transport(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, transport::transport_read);
    lua_setfield(L, -2, "read");
    lua_pushcfunction(L, transport::transport_write);
    lua_setfield(L, -2, "write");
    lua_pushcfunction(L, transport::transport_register_stdio_lua);
    lua_setfield(L, -2, "register_stdio");
    lua_pushcfunction(L, transport_disconnect_lua);
    lua_setfield(L, -2, "disconnect");
    return 1;
}

static ::bee::lua::callfunc _init_transport(::bee::lua::register_module, "transport", luaopen_transport);
