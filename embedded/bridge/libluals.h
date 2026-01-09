// embedded/bridge/libluals.h
#ifndef LIBLUALS_H
#define LIBLUALS_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to Lua engine
typedef struct luals_engine luals_engine_t;

// Completion item
typedef struct {
    char* label;
    int kind;
    char* detail;
    char* documentation;
    char* insert_text;
} luals_completion_t;

// Diagnostic
typedef struct {
    int line;
    int start_char;
    int end_char;
    int severity;  // 1=Error, 2=Warning, 3=Info, 4=Hint
    char* message;
    char* source;
} luals_diagnostic_t;

// Hover info
typedef struct {
    char* content;
    bool is_markdown;
} luals_hover_t;

// Location
typedef struct {
    char* uri;
    int line;
    int character;
} luals_location_t;

// Engine lifecycle
luals_engine_t* luals_engine_create(const char* workspace_path);
void luals_engine_destroy(luals_engine_t* engine);

// File management
void luals_set_file_content(luals_engine_t* engine, const char* uri, const char* text);

// LSP features
luals_completion_t* luals_get_completions(
    luals_engine_t* engine,
    const char* uri,
    int line,
    int character,
    int* out_count
);

luals_diagnostic_t* luals_get_diagnostics(
    luals_engine_t* engine,
    const char* uri,
    int* out_count
);

luals_hover_t* luals_get_hover(
    luals_engine_t* engine,
    const char* uri,
    int line,
    int character
);

luals_location_t* luals_get_definition(
    luals_engine_t* engine,
    const char* uri,
    int line,
    int character,
    int* out_count
);

// Memory management
void luals_completions_free(luals_completion_t* completions, int count);
void luals_diagnostics_free(luals_diagnostic_t* diagnostics, int count);
void luals_hover_free(luals_hover_t* hover);
void luals_locations_free(luals_location_t* locations, int count);

#ifdef __cplusplus
}
#endif

#endif // LIBLUALS_H
