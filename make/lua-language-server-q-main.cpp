/**
 * lua-language-server-q: Wraps liblualsp, bridges stdio <-> input queue and output callback.
 * Same external interface as lua-language-server (stdio).
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

extern "C" {
void LSPInit(int argc, char** argv, void (*output_cb)(const char* data, size_t len));
void LSPWriteInput(const char* data, size_t len);
void LSPDisconnect(void);
void LSPMain(void);
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
    LSPInit(argc, argv, output_to_stdout);

    std::thread worker(LSPMain);

    reader_pump();

    worker.join();
    return 0;
}
