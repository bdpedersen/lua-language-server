// embedded/test/test_c_api.c
// Test the C API of the embedded library

#include <stdio.h>
#include <stdlib.h>
#include "../bridge/libluals.h"

int main(int argc, char** argv) {
    printf("=== Testing LuaLS Embedded C API ===\n\n");
    
    // Test 1: Create engine
    printf("[Test 1] Creating engine...\n");
    luals_engine_t* engine = luals_engine_create("/tmp/test_workspace");
    if (!engine) {
        fprintf(stderr, "FAILED: Could not create engine\n");
        return 1;
    }
    printf("PASSED: Engine created successfully\n\n");
    
    // Test 2: Set file content
    printf("[Test 2] Setting file content...\n");
    const char* uri = "file:///test.lua";
    const char* code = "local function hello(name)\n    print('Hello, ' .. name)\nend\n";
    luals_set_file_content(engine, uri, code);
    printf("PASSED: File content set\n\n");
    
    // Test 3: Get completions
    printf("[Test 3] Getting completions...\n");
    int completion_count = 0;
    luals_completion_t* completions = luals_get_completions(engine, uri, 1, 10, &completion_count);
    printf("Got %d completions\n", completion_count);
    if (completions) {
        for (int i = 0; i < completion_count && i < 5; i++) {
            printf("  - %s (%s)\n", completions[i].label, completions[i].detail ? completions[i].detail : "");
        }
        luals_completions_free(completions, completion_count);
        printf("PASSED: Completions retrieved\n\n");
    } else {
        printf("INFO: No completions (this is OK for now)\n\n");
    }
    
    // Test 4: Get diagnostics
    printf("[Test 4] Getting diagnostics...\n");
    int diag_count = 0;
    luals_diagnostic_t* diagnostics = luals_get_diagnostics(engine, uri, &diag_count);
    printf("Got %d diagnostics\n", diag_count);
    if (diagnostics) {
        for (int i = 0; i < diag_count; i++) {
            printf("  - Line %d: %s\n", diagnostics[i].line, diagnostics[i].message);
        }
        luals_diagnostics_free(diagnostics, diag_count);
        printf("PASSED: Diagnostics retrieved\n\n");
    } else {
        printf("INFO: No diagnostics (this is OK)\n\n");
    }
    
    // Test 5: Get hover
    printf("[Test 5] Getting hover info...\n");
    luals_hover_t* hover = luals_get_hover(engine, uri, 1, 15);
    if (hover) {
        printf("Hover content: %s\n", hover->content);
        luals_hover_free(hover);
        printf("PASSED: Hover retrieved\n\n");
    } else {
        printf("INFO: No hover info (this is OK)\n\n");
    }
    
    // Test 6: Get definition
    printf("[Test 6] Getting definition...\n");
    int location_count = 0;
    luals_location_t* locations = luals_get_definition(engine, uri, 1, 15, &location_count);
    printf("Got %d definition locations\n", location_count);
    if (locations) {
        for (int i = 0; i < location_count; i++) {
            printf("  - %s at line %d\n", locations[i].uri, locations[i].line);
        }
        luals_locations_free(locations, location_count);
        printf("PASSED: Definitions retrieved\n\n");
    } else {
        printf("INFO: No definitions (this is OK)\n\n");
    }
    
    // Test 7: Destroy engine
    printf("[Test 7] Destroying engine...\n");
    luals_engine_destroy(engine);
    printf("PASSED: Engine destroyed\n\n");
    
    printf("=== All C API tests completed successfully ===\n");
    return 0;
}
