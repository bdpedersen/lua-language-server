/**
 * lua-language-server-q: Wraps liblualsp, bridges stdio <-> input queue and output callback.
 * Same external interface as lua-language-server (stdio).
 * Injects --resource-dir when not present, using executable location to find script/.
 */

#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#    define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_WIN32)
#    include <3rd/lua-patch/bee_utf8_crt.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <vector>

#if defined(_WIN32)
#    include <io.h>
#    define LSP_READ(fd, buf, len) _read((fd), (buf), (unsigned)(len))
#else
#    include <unistd.h>
#    define LSP_READ(fd, buf, len) read((fd), (buf), (len))
#endif

#include <bee/nonstd/filesystem.h>
#include <bee/sys/path.h>

extern "C" {
void LSPInit(int argc, char** argv, void (*output_cb)(const char* data, size_t len));
void LSPWriteInput(const char* data, size_t len);
void LSPDisconnect(void);
void LSPMain(void);
}

/** Returns true if argv contains --resource-dir or --resource-dir=<path>. */
static bool has_resource_dir(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* v = argv[i];
        if (strncmp(v, "--resource-dir=", 15) == 0)
            return true;
        if (strcmp(v, "--resource-dir") == 0 && i + 1 < argc)
            return true;
    }
    return false;
}

/** Compute resource dir from executable path. Prefer parent (exe in bin/) then exe dir (flat). */
static std::string compute_resource_dir() {
    auto exe = bee::sys::exe_path();
    if (!exe)
        return ".";
    fs::path parent = exe->parent_path();
    fs::path grandparent = parent.parent_path();
    if (fs::exists(parent / "script"))
        return parent.string();
    if (fs::exists(grandparent / "script"))
        return grandparent.string();
    return parent.string();
}

static void output_to_stdout(const char* data, size_t len) {
    if (data && len > 0) {
        fwrite(data, 1, len, stdout);
        fflush(stdout);
    }
}

/**
 * Reader pump: runs on main thread. Reads stdin, feeds LSPWriteInput.
 * On EOF, calls LSPDisconnect so LSPMain (worker) returns.
 */
static void reader_pump() {
    std::vector<char> buf(4096);
    while (true) {
        ssize_t n = LSP_READ(0, buf.data(), buf.size());
        if (n > 0) {
            LSPWriteInput(buf.data(), static_cast<size_t>(n));
        } else {
            break;
        }
    }
    LSPDisconnect();
}

#if defined(_WIN32)
extern "C" {
#    include "3rd/lua-patch/bee_utf8_main.c"
}
extern "C" int utf8_main(int argc, char** argv) {
#else
int main(int argc, char** argv) {
#endif
    int init_argc = argc;
    char** init_argv = argv;
    std::vector<std::string> resource_dir_arg;
    std::vector<char*> injected_argv;

    if (!has_resource_dir(argc, argv)) {
        std::string dir = compute_resource_dir();
        resource_dir_arg.push_back("--resource-dir=" + dir);
        injected_argv.push_back(argv[0]);
        injected_argv.push_back(&resource_dir_arg[0][0]);
        for (int i = 1; i < argc; i++)
            injected_argv.push_back(argv[i]);
        init_argc = static_cast<int>(injected_argv.size());
        init_argv = injected_argv.data();
    }

    LSPInit(init_argc, init_argv, output_to_stdout);

    std::thread worker(LSPMain);

    reader_pump();

    worker.join();
    return 0;
}
