#pragma once

#include <cstdio>

struct UiCiResult {
    bool ok{true};
    int failed{0};
};

inline void ui_ci_emit(const char* name, bool ok, const char* reason) {
    if (ok) {
        std::printf("[ui-ci] case=%s ok=1\n", name);
    } else {
        std::printf("[ui-ci] case=%s ok=0 reason=%s\n", name, reason ? reason : "unknown");
    }
}

void run_object_tree_input_regressions(UiCiResult& res);
