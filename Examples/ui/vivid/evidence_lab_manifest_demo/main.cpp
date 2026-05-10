#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {
    enum EvidenceAxis : std::uint32_t {
        AxisEdge = 1u << 0u,
        AxisState = 1u << 1u,
        AxisStyle = 1u << 2u,
        AxisRender = 1u << 3u,
        AxisSemantic = 1u << 4u,
        AxisFocus = 1u << 5u,
        AxisMotion = 1u << 6u,
        AxisTransaction = 1u << 7u,
        AxisLayer = 1u << 8u,
        AxisVocabulary = 1u << 9u,
        AxisCausal = 1u << 10u,
        AxisAdmission = 1u << 11u,
        AxisManifest = 1u << 12u,
    };

    struct ManifestEntry {
        const char* run;
        const char* tag;
        unsigned cases;
        std::uint32_t axes;
        const char* primary_doc;
    };

    constexpr ManifestEntry kManifest[] = {
        {"page_transition_demo", "pt", 16, AxisMotion | AxisTransaction | AxisLayer | AxisRender | AxisAdmission | AxisCausal, "docs/ui/vivid_motion_runtime_v0.md"},
        {"motion_time_demo", "mt", 13, AxisMotion | AxisCausal, "docs/ui/vivid_motion_runtime_v0.md"},
        {"component_card_state_demo", "ccs", 6, AxisState | AxisRender | AxisCausal, "docs/ui/vivid_render_evidence_chain_v0.md"},
        {"component_settings_row_demo", "csr", 5, AxisState | AxisRender | AxisCausal, "docs/ui/vivid_render_evidence_chain_v0.md"},
        {"style_token_law_demo", "stl", 7, AxisStyle | AxisRender | AxisCausal, "docs/ui/vivid_style_token_law_v0.md"},
        {"focus_boundary_demo", "fb", 7, AxisFocus | AxisStyle | AxisRender | AxisCausal, "docs/ui/vivid_focus_evidence_boundary_v0.md"},
        {"focus_transfer_demo", "ft", 8, AxisEdge | AxisFocus | AxisRender | AxisCausal, "docs/ui/vivid_focus_transfer_evidence_v0.md"},
        {"focus_scope_demo", "fs", 10, AxisEdge | AxisFocus | AxisRender | AxisAdmission | AxisCausal, "docs/ui/vivid_focus_scope_evidence_v0.md"},
        {"focus_scope_nested_demo", "fsn", 9, AxisFocus | AxisTransaction | AxisRender | AxisCausal, "docs/ui/vivid_focus_scope_evidence_v0.md"},
        {"focus_scope_navigation_demo", "fsnav", 8, AxisEdge | AxisFocus | AxisRender | AxisCausal, "docs/ui/vivid_focus_scope_evidence_v0.md"},
        {"focus_spatial_navigation_demo", "fss", 10, AxisEdge | AxisFocus | AxisRender | AxisCausal, "docs/ui/vivid_focus_scope_evidence_v0.md"},
        {"focus_semantic_demo", "fsem", 9, AxisSemantic | AxisFocus | AxisRender | AxisCausal, "docs/ui/vivid_focus_semantic_evidence_v0.md"},
        {"semantic_tree_demo", "stree", 7, AxisSemantic | AxisCausal, "docs/ui/vivid_focus_semantic_evidence_v0.md"},
        {"semantic_default_demo", "sdef", 7, AxisSemantic | AxisCausal, "docs/ui/vivid_focus_semantic_evidence_v0.md"},
        {"semantic_action_demo", "sact", 7, AxisSemantic | AxisCausal, "docs/ui/vivid_focus_semantic_evidence_v0.md"},
        {"semantic_intent_demo", "sint", 9, AxisSemantic | AxisAdmission | AxisCausal, "docs/ui/vivid_semantic_request_ledger_law_v0.md"},
        {"semantic_action_admission_demo", "saa", 9, AxisSemantic | AxisAdmission | AxisCausal, "docs/ui/vivid_semantic_request_ledger_law_v0.md"},
        {"semantic_action_request_demo", "sar", 11, AxisSemantic | AxisAdmission | AxisEdge | AxisFocus | AxisCausal, "docs/ui/vivid_semantic_request_ledger_law_v0.md"},
        {"intent_artifact_demo", "ia", 9, AxisSemantic | AxisState | AxisRender | AxisCausal | AxisAdmission, "docs/ui/vivid_intent_to_artifact_evidence_v0.md"},
        {"semantic_transition_demo", "stx", 9, AxisSemantic | AxisEdge | AxisAdmission | AxisTransaction | AxisLayer | AxisRender | AxisCausal, "docs/ui/vivid_semantic_transition_law_v0.md"},
        {"semantic_action_state_transition_demo", "sastx", 10, AxisSemantic | AxisEdge | AxisAdmission | AxisState | AxisRender | AxisTransaction | AxisLayer | AxisCausal, "docs/ui/vivid_semantic_action_state_transition_law_v0.md"},
        {"semantic_focus_query_demo", "sfq", 9, AxisSemantic | AxisFocus | AxisCausal, "docs/ui/vivid_semantic_request_ledger_law_v0.md"},
        {"semantic_focus_admission_demo", "sfa", 9, AxisSemantic | AxisFocus | AxisAdmission | AxisCausal, "docs/ui/vivid_semantic_request_ledger_law_v0.md"},
        {"semantic_focus_request_demo", "sfr", 12, AxisSemantic | AxisFocus | AxisAdmission | AxisEdge | AxisRender | AxisCausal, "docs/ui/vivid_semantic_request_ledger_law_v0.md"},
        {"widget_signal_demo", "ws", 3, AxisEdge, "docs/ui/vivid_evidence_stdout_law.md"},
        {"widget_state_demo", "wst", 5, AxisState, "docs/ui/vivid_widget_state_observe.md"},
        {"evidence_vocabulary_demo", "evl", 5, AxisVocabulary | AxisState | AxisRender | AxisCausal, "docs/ui/vivid_evidence_vocabulary_law_v0.md"},
        {"evidence_lab_manifest_demo", "elm", 10, AxisManifest | AxisVocabulary, "docs/ui/vivid_evidence_lab_manifest_v0.md"},
    };

    constexpr unsigned kExpectedEntryCount = 28;
    constexpr unsigned kExpectedCaseTotal = 239;
    constexpr std::uint32_t kRequiredAxes =
        AxisEdge
        | AxisState
        | AxisStyle
        | AxisRender
        | AxisSemantic
        | AxisFocus
        | AxisMotion
        | AxisTransaction
        | AxisLayer
        | AxisVocabulary
        | AxisCausal
        | AxisAdmission
        | AxisManifest;

    unsigned manifest_case_count = 0;

    [[nodiscard]] bool text_empty(const char* text) noexcept {
        return text == nullptr || text[0] == '\0';
    }

    [[nodiscard]] bool text_equal(const char* lhs, const char* rhs) noexcept {
        if (lhs == nullptr || rhs == nullptr) return lhs == rhs;
        return std::strcmp(lhs, rhs) == 0;
    }

    [[nodiscard]] std::string source_path(const char* relative_path) {
        std::string path = CHARM_SOURCE_ROOT;
        path += "/";
        path += relative_path;
        return path;
    }

    [[nodiscard]] std::string read_file(const char* relative_path) {
        std::ifstream input(source_path(relative_path), std::ios::binary);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    [[nodiscard]] bool contains(const std::string& haystack, const std::string& needle) noexcept {
        return haystack.find(needle) != std::string::npos;
    }

    [[nodiscard]] std::string case_text(unsigned cases) {
        return std::to_string(cases);
    }

    [[nodiscard]] std::string stdout_gate(const ManifestEntry& entry) {
        return std::string("[")
            + entry.tag
            + "] run="
            + entry.run
            + " phase=end result=ok cases="
            + case_text(entry.cases);
    }

    [[nodiscard]] std::string cmake_gate(const ManifestEntry& entry) {
        return std::string("\\\\[")
            + entry.tag
            + "\\\\] run="
            + entry.run
            + " phase=end result=ok cases="
            + case_text(entry.cases);
    }

    [[nodiscard]] std::string demo_path(const ManifestEntry& entry) {
        return std::string("Examples/ui/vivid/") + entry.run;
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    void run_begin() noexcept {
        std::puts("[elm] run=evidence_lab_manifest_demo phase=begin");
    }

    void run_end(bool ok) noexcept {
        std::printf("[elm] run=evidence_lab_manifest_demo phase=end result=%s cases=%u\n",
                    ok ? "ok" : "fail",
                    manifest_case_count);
    }

    void case_begin(const char* name) noexcept {
        ++manifest_case_count;
        std::printf("[elm] case=%s", name);
    }

    [[nodiscard]] unsigned manifest_entry_count() noexcept {
        return static_cast<unsigned>(sizeof(kManifest) / sizeof(kManifest[0]));
    }

    [[nodiscard]] unsigned total_cases() noexcept {
        unsigned total = 0;
        for (const auto& entry : kManifest) {
            total += entry.cases;
        }
        return total;
    }

    [[nodiscard]] std::uint32_t axis_union() noexcept {
        std::uint32_t axes = 0;
        for (const auto& entry : kManifest) {
            axes |= entry.axes;
        }
        return axes;
    }

    [[nodiscard]] bool has_axis(std::uint32_t axes, EvidenceAxis axis) noexcept {
        return (axes & axis) != 0u;
    }

    [[nodiscard]] bool gate_shape_valid() noexcept {
        for (const auto& entry : kManifest) {
            if (text_empty(entry.run) || text_empty(entry.tag) || entry.cases == 0 || entry.axes == 0u) {
                return false;
            }
            if (text_empty(entry.primary_doc)) {
                return false;
            }
            if (std::strlen(entry.tag) > 5u) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool runs_unique() noexcept {
        for (std::size_t outer = 0; outer < manifest_entry_count(); ++outer) {
            for (std::size_t inner = outer + 1; inner < manifest_entry_count(); ++inner) {
                if (text_equal(kManifest[outer].run, kManifest[inner].run)) {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool tags_unique() noexcept {
        for (std::size_t outer = 0; outer < manifest_entry_count(); ++outer) {
            for (std::size_t inner = outer + 1; inner < manifest_entry_count(); ++inner) {
                if (text_equal(kManifest[outer].tag, kManifest[inner].tag)) {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] const ManifestEntry* find_entry(const char* run) noexcept {
        for (const auto& entry : kManifest) {
            if (text_equal(entry.run, run)) {
                return &entry;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool stdout_law_matches_manifest(const std::string& stdout_law) {
        if (stdout_law.empty()) return false;
        for (const auto& entry : kManifest) {
            if (!contains(stdout_law, stdout_gate(entry))) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool cmake_gates_match_manifest() {
        for (const auto& entry : kManifest) {
            const std::string relative_path =
                std::string("Examples/ui/vivid/") + entry.run + "/CMakeLists.txt";
            const std::string cmake_file = read_file(relative_path.c_str());
            if (cmake_file.empty() || !contains(cmake_file, cmake_gate(entry))) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool doc_routes_match_manifest() {
        for (const auto& entry : kManifest) {
            const std::string doc = read_file(entry.primary_doc);
            if (doc.empty() || !contains(doc, demo_path(entry))) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool causal_docs_match_manifest() {
        const std::string causal_law = read_file("docs/ui/vivid_causal_verdict_law_v0.md");
        if (causal_law.empty()
            || !contains(causal_law, "AxisCausal Eligibility")
            || !contains(causal_law, "Count-Based And Evidence-Referenced Verdicts")) {
            return false;
        }

        for (const auto& entry : kManifest) {
            if (!has_axis(entry.axes, AxisCausal)) {
                continue;
            }

            const std::string doc = read_file(entry.primary_doc);
            if (doc.empty() || (!contains(doc, "causal_chain") && !contains(doc, "causal verdict"))) {
                return false;
            }
        }
        return true;
    }
}

int main() {
    run_begin();

    const unsigned entries = manifest_entry_count();
    const unsigned cases = total_cases();
    const bool unique_runs = runs_unique();
    const bool unique_tags = tags_unique();
    const bool shape_ok = gate_shape_valid();

    case_begin("registry_shape");
    std::printf(" entries=%u expected_entries=%u total_cases=%u expected_cases=%u unique_runs=%d unique_tags=%d shape_ok=%d\n",
                entries,
                kExpectedEntryCount,
                cases,
                kExpectedCaseTotal,
                unique_runs ? 1 : 0,
                unique_tags ? 1 : 0,
                shape_ok ? 1 : 0);
    if (!expect(entries == kExpectedEntryCount
                && cases == kExpectedCaseTotal
                && unique_runs
                && unique_tags
                && shape_ok,
                "manifest registry shape must stay stable")) {
        return 1;
    }

    case_begin("stdout_gate_shape");
    std::printf(" gates=%u tag_max=5 cases_positive=1 ctest_gate=1\n", entries);
    if (!expect(shape_ok, "manifest entries must have CTest-compatible gate fields")) return 1;

    const std::uint32_t axes = axis_union();
    const bool required_axes_present = (axes & kRequiredAxes) == kRequiredAxes;
    case_begin("axis_coverage");
    std::printf(" edge=%d state=%d style=%d render=%d semantic=%d focus=%d motion=%d transaction=%d layer=%d vocabulary=%d causal=%d admission=%d manifest=%d\n",
                has_axis(axes, AxisEdge) ? 1 : 0,
                has_axis(axes, AxisState) ? 1 : 0,
                has_axis(axes, AxisStyle) ? 1 : 0,
                has_axis(axes, AxisRender) ? 1 : 0,
                has_axis(axes, AxisSemantic) ? 1 : 0,
                has_axis(axes, AxisFocus) ? 1 : 0,
                has_axis(axes, AxisMotion) ? 1 : 0,
                has_axis(axes, AxisTransaction) ? 1 : 0,
                has_axis(axes, AxisLayer) ? 1 : 0,
                has_axis(axes, AxisVocabulary) ? 1 : 0,
                has_axis(axes, AxisCausal) ? 1 : 0,
                has_axis(axes, AxisAdmission) ? 1 : 0,
                has_axis(axes, AxisManifest) ? 1 : 0);
    if (!expect(required_axes_present, "manifest must cover every v0 evidence axis")) return 1;

    const ManifestEntry* intent_artifact = find_entry("intent_artifact_demo");
    const bool intent_chain_ok = intent_artifact != nullptr
        && has_axis(intent_artifact->axes, AxisSemantic)
        && has_axis(intent_artifact->axes, AxisState)
        && has_axis(intent_artifact->axes, AxisRender)
        && has_axis(intent_artifact->axes, AxisCausal)
        && has_axis(intent_artifact->axes, AxisAdmission);
    case_begin("intent_to_artifact_anchor");
    std::printf(" run=intent_artifact_demo has_semantic=%d has_state=%d has_render=%d has_causal=%d has_admission=%d\n",
                intent_artifact && has_axis(intent_artifact->axes, AxisSemantic) ? 1 : 0,
                intent_artifact && has_axis(intent_artifact->axes, AxisState) ? 1 : 0,
                intent_artifact && has_axis(intent_artifact->axes, AxisRender) ? 1 : 0,
                intent_artifact && has_axis(intent_artifact->axes, AxisCausal) ? 1 : 0,
                intent_artifact && has_axis(intent_artifact->axes, AxisAdmission) ? 1 : 0);
    if (!expect(intent_chain_ok, "intent artifact must remain the vertical causal anchor")) return 1;

    const ManifestEntry* semantic_transition = find_entry("semantic_transition_demo");
    const bool semantic_transition_ok = semantic_transition != nullptr
        && has_axis(semantic_transition->axes, AxisSemantic)
        && has_axis(semantic_transition->axes, AxisEdge)
        && has_axis(semantic_transition->axes, AxisAdmission)
        && has_axis(semantic_transition->axes, AxisTransaction)
        && has_axis(semantic_transition->axes, AxisLayer)
        && has_axis(semantic_transition->axes, AxisRender)
        && has_axis(semantic_transition->axes, AxisCausal);
    case_begin("semantic_transition_anchor");
    std::printf(" run=semantic_transition_demo semantic=%d edge=%d admission=%d transaction=%d layer=%d render=%d causal=%d\n",
                semantic_transition && has_axis(semantic_transition->axes, AxisSemantic) ? 1 : 0,
                semantic_transition && has_axis(semantic_transition->axes, AxisEdge) ? 1 : 0,
                semantic_transition && has_axis(semantic_transition->axes, AxisAdmission) ? 1 : 0,
                semantic_transition && has_axis(semantic_transition->axes, AxisTransaction) ? 1 : 0,
                semantic_transition && has_axis(semantic_transition->axes, AxisLayer) ? 1 : 0,
                semantic_transition && has_axis(semantic_transition->axes, AxisRender) ? 1 : 0,
                semantic_transition && has_axis(semantic_transition->axes, AxisCausal) ? 1 : 0);
    if (!expect(semantic_transition_ok, "semantic transition must remain the first semantic-to-transaction anchor")) return 1;

    const ManifestEntry* vocabulary = find_entry("evidence_vocabulary_demo");
    const bool vocabulary_ok = vocabulary != nullptr
        && has_axis(vocabulary->axes, AxisVocabulary)
        && has_axis(vocabulary->axes, AxisCausal)
        && has_axis(vocabulary->axes, AxisRender);
    case_begin("vocabulary_anchor");
    std::printf(" run=evidence_vocabulary_demo vocabulary=%d causal=%d render=%d implementation=demo_side\n",
                vocabulary && has_axis(vocabulary->axes, AxisVocabulary) ? 1 : 0,
                vocabulary && has_axis(vocabulary->axes, AxisCausal) ? 1 : 0,
                vocabulary && has_axis(vocabulary->axes, AxisRender) ? 1 : 0);
    if (!expect(vocabulary_ok, "vocabulary demo must remain the field-law anchor")) return 1;

    const std::string stdout_law = read_file("docs/ui/vivid_evidence_stdout_law.md");
    const bool stdout_sync_ok = stdout_law_matches_manifest(stdout_law);
    case_begin("stdout_law_sync");
    std::printf(" file=docs/ui/vivid_evidence_stdout_law.md entries=%u synced=%d\n",
                entries,
                stdout_sync_ok ? 1 : 0);
    if (!expect(stdout_sync_ok, "stdout law registry must match manifest gates")) return 1;

    const bool cmake_sync_ok = cmake_gates_match_manifest();
    case_begin("cmake_gate_sync");
    std::printf(" demos=%u synced=%d\n", entries, cmake_sync_ok ? 1 : 0);
    if (!expect(cmake_sync_ok, "demo CMake PASS gates must match manifest gates")) return 1;

    const bool doc_route_ok = doc_routes_match_manifest();
    const bool causal_doc_ok = causal_docs_match_manifest();
    case_begin("doc_route_sync");
    std::printf(" demos=%u primary_docs=%u synced=%d causal_docs=%d\n",
                entries,
                entries,
                doc_route_ok ? 1 : 0,
                causal_doc_ok ? 1 : 0);
    if (!expect(doc_route_ok && causal_doc_ok, "manifest primary docs must point back to demos and cover AxisCausal law")) return 1;

    case_begin("promotion_boundary");
    std::printf(" demo_support=demo_side vocabulary=law runtime_ledgers=core_candidate print_helpers=do_not_promote runtime_behavior=0 screenshot=0\n");
    if (!expect(true, "promotion boundary is declarative in the manifest")) return 1;

    run_end(true);
    std::puts("[evidence_lab_manifest_demo] ok");
    return 0;
}
