#include "profiles/profile_evidence.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace h747::host_profile_compare {

enum class BindingCompareStatus : std::uint8_t {
    ok,
    capability_mismatch,
    same_provider,
    missing_explicit_binding,
    evidence_error,
};

struct BindingCompareRow {
    std::string_view capability{};
    std::string_view host_provider{};
    std::string_view h747_provider{};
    std::array<h747::profiles::EvidenceField, 6> host_fields{};
    std::array<h747::profiles::EvidenceField, 6> h747_fields{};
    std::size_t host_field_count{0U};
    std::size_t h747_field_count{0U};
    BindingCompareStatus status{BindingCompareStatus::ok};
};

struct ProfileCompareReport {
    std::string_view host_profile{};
    std::string_view h747_profile{};
    std::string_view host_board{};
    std::string_view h747_board{};
    bool profiles_differ{false};
    bool boards_differ{false};
    std::array<BindingCompareRow, 4> bindings{};
    std::size_t binding_count{0U};
};

using ProfileExplainReport = ProfileCompareReport;

class TextBuffer {
public:
    [[nodiscard]] bool append(const std::string_view text) noexcept {
        const auto remaining = bytes_.size() - used_;
        if (text.size() > remaining) {
            return false;
        }
        std::memcpy(bytes_.data() + used_, text.data(), text.size());
        used_ += text.size();
        return true;
    }

    [[nodiscard]] std::string_view view() const noexcept {
        return {bytes_.data(), used_};
    }

private:
    std::array<char, 1024U> bytes_{};
    std::size_t used_{0U};
};

[[nodiscard]] constexpr bool has_field(const h747::profiles::EvidenceFrame& frame,
                                       const std::string_view key,
                                       const std::string_view value) noexcept {
    for (std::size_t i = 0U; i < frame.field_count; ++i) {
        if (frame.fields[i].key == key && frame.fields[i].value == value) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr BindingCompareStatus compare_binding(
    const h747::profiles::EvidenceFrame& host,
    const h747::profiles::EvidenceFrame& h747) noexcept {
    if (host.status != h747::profiles::EvidenceStatus::ok ||
        h747.status != h747::profiles::EvidenceStatus::ok) {
        return BindingCompareStatus::evidence_error;
    }
    if (host.capability != h747.capability) {
        return BindingCompareStatus::capability_mismatch;
    }
    if (host.provider == h747.provider) {
        return BindingCompareStatus::same_provider;
    }
    if (!has_field(host, "selection", "explicit_binding") ||
        !has_field(h747, "selection", "explicit_binding")) {
        return BindingCompareStatus::missing_explicit_binding;
    }
    return BindingCompareStatus::ok;
}

[[nodiscard]] constexpr bool is_control_field(const std::string_view key) noexcept {
    return key == "profile" || key == "selection";
}

[[nodiscard]] constexpr std::string_view field_value(const BindingCompareRow& row,
                                                     const bool host,
                                                     const std::string_view key) noexcept {
    const auto& fields = host ? row.host_fields : row.h747_fields;
    const auto count = host ? row.host_field_count : row.h747_field_count;
    for (std::size_t i = 0U; i < count; ++i) {
        if (fields[i].key == key) {
            return fields[i].value;
        }
    }
    return {};
}

[[nodiscard]] constexpr ProfileCompareReport compare_profiles(
    const h747::profiles::ProfileEvidence& host,
    const h747::profiles::ProfileEvidence& h747) noexcept {
    ProfileCompareReport report{
        .host_profile = host.profile,
        .h747_profile = h747.profile,
        .host_board = host.board,
        .h747_board = h747.board,
        .profiles_differ = host.profile != h747.profile,
        .boards_differ = host.board != h747.board,
        .binding_count = host.bindings.size(),
    };

    for (std::size_t i = 0U; i < report.binding_count; ++i) {
        report.bindings[i] = BindingCompareRow{
            .capability = host.bindings[i].capability,
            .host_provider = host.bindings[i].provider,
            .h747_provider = h747.bindings[i].provider,
            .host_fields = host.bindings[i].fields,
            .h747_fields = h747.bindings[i].fields,
            .host_field_count = host.bindings[i].field_count,
            .h747_field_count = h747.bindings[i].field_count,
            .status = compare_binding(host.bindings[i], h747.bindings[i]),
        };
    }
    return report;
}

[[nodiscard]] constexpr ProfileExplainReport project_profile_explain(
    const h747::profiles::ProfileEvidence& host,
    const h747::profiles::ProfileEvidence& h747) noexcept {
    return compare_profiles(host, h747);
}

[[nodiscard]] constexpr std::string_view status_text(const BindingCompareStatus status) noexcept {
    switch (status) {
    case BindingCompareStatus::ok:
        return "ok";
    case BindingCompareStatus::capability_mismatch:
        return "capability_mismatch";
    case BindingCompareStatus::same_provider:
        return "same_provider";
    case BindingCompareStatus::missing_explicit_binding:
        return "missing_explicit_binding";
    case BindingCompareStatus::evidence_error:
        return "evidence_error";
    }
    return "unknown";
}

[[nodiscard]] bool append_fact_fields(TextBuffer& output,
                                      const std::string_view prefix,
                                      const std::array<h747::profiles::EvidenceField, 6>& fields,
                                      const std::size_t field_count) noexcept {
    bool ok = true;
    for (std::size_t i = 0U; i < field_count; ++i) {
        const auto& field = fields[i];
        if (is_control_field(field.key)) {
            continue;
        }
        ok = output.append(" ") && output.append(prefix) &&
             output.append(".") && output.append(field.key) &&
             output.append("=") && output.append(field.value) && ok;
    }
    return ok;
}

[[nodiscard]] bool format_profile_compare(const ProfileCompareReport& report, TextBuffer& output) noexcept {
    bool ok = true;
    ok = output.append("profile_compare\n") && ok;
    ok = output.append("host_profile=") && output.append(report.host_profile) &&
         output.append(" h747_profile=") && output.append(report.h747_profile) &&
         output.append("\n") && ok;
    ok = output.append("host_board=") && output.append(report.host_board) &&
         output.append(" h747_board=") && output.append(report.h747_board) &&
         output.append("\n") && ok;

    for (std::size_t i = 0U; i < report.binding_count; ++i) {
        const auto& row = report.bindings[i];
        ok = output.append("binding capability=") && output.append(row.capability) &&
             output.append(" host_provider=") && output.append(row.host_provider) &&
             output.append(" h747_provider=") && output.append(row.h747_provider) &&
             output.append(" status=") && output.append(status_text(row.status)) && ok;
        ok = append_fact_fields(output, "host", row.host_fields, row.host_field_count) && ok;
        ok = append_fact_fields(output, "h747", row.h747_fields, row.h747_field_count) && ok;
        ok = output.append("\n") && ok;
    }
    return ok;
}

[[nodiscard]] constexpr bool report_ok(const ProfileCompareReport& report) noexcept {
    if (!report.profiles_differ || !report.boards_differ || report.binding_count != 4U) {
        return false;
    }
    for (std::size_t i = 0U; i < report.binding_count; ++i) {
        if (report.bindings[i].status != BindingCompareStatus::ok) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool contains(const std::string_view haystack, const std::string_view needle) noexcept {
    return haystack.find(needle) != std::string_view::npos;
}

[[nodiscard]] constexpr bool explain_report_ok(const ProfileExplainReport& report) noexcept {
    if (!report_ok(report)) {
        return false;
    }

    const auto& display = report.bindings[2];
    const auto& input = report.bindings[3];
    return display.capability == "RasterDisplay.primary_display" &&
           input.capability == "Input.primary_input" &&
           field_value(display, true, "mode") != field_value(display, false, "mode") &&
           field_value(display, true, "format") == "argb8888" &&
           field_value(display, false, "format") == "argb8888" &&
           field_value(display, true, "buffer_policy") != field_value(display, false, "buffer_policy") &&
           field_value(input, true, "source") == "null_input" &&
           field_value(input, false, "source") == "h747_input_service" &&
           field_value(input, true, "pointer") != field_value(input, false, "pointer") &&
           field_value(input, true, "encoders") != field_value(input, false, "encoders");
}

[[nodiscard]] bool text_has_explain_facts(const std::string_view text) noexcept {
    return contains(text, "capability=RasterDisplay.primary_display") &&
           contains(text, "host_provider=host_framebuffer") &&
           contains(text, "h747_provider=h747_raster_display_service") &&
           contains(text, "host.mode=180x320") &&
           contains(text, "h747.mode=720x1280") &&
           contains(text, "host.buffer_policy=single_memory_framebuffer") &&
           contains(text, "h747.buffer_policy=double_buffer_vblank_reload") &&
           contains(text, "host.source=null_input") &&
           contains(text, "h747.source=h747_input_service") &&
           contains(text, "status=ok");
}

[[nodiscard]] bool expect(const bool condition, const char* message) noexcept {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

} // namespace h747::host_profile_compare

int main() {
    using namespace h747::host_profile_compare;

    constexpr auto host = h747::profiles::host_player_profile_evidence();
    constexpr auto h747 = h747::profiles::h747_player_profile_evidence();
    constexpr auto report = project_profile_explain(host, h747);

    static_assert(report.binding_count == 4U);
    static_assert(report.bindings[0].capability == "TextSink.log");
    static_assert(report.bindings[1].capability == "Clock.monotonic_time");
    static_assert(report.bindings[2].capability == "RasterDisplay.primary_display");
    static_assert(report.bindings[3].capability == "Input.primary_input");
    static_assert(report_ok(report));
    static_assert(explain_report_ok(report));

    TextBuffer output{};
    if (!expect(format_profile_compare(report, output), "profile compare presentation fits output buffer")) return 1;
    const auto text = output.view();

    if (!expect(report.profiles_differ, "host and h747 profiles are intentionally different")) return 1;
    if (!expect(report.boards_differ, "host and h747 boards are intentionally different")) return 1;
    if (!expect(report.bindings[2].host_provider == "host_framebuffer", "host display provider is evidence-only")) return 1;
    if (!expect(report.bindings[2].h747_provider == "h747_raster_display_service", "h747 display provider is evidence-only")) return 1;
    if (!expect(field_value(report.bindings[2], true, "mode") == "180x320", "host display mode is reported")) return 1;
    if (!expect(field_value(report.bindings[2], false, "mode") == "720x1280", "h747 display mode is reported")) return 1;
    if (!expect(field_value(report.bindings[2], true, "mode") != field_value(report.bindings[2], false, "mode"), "display modes differ by profile")) return 1;
    if (!expect(field_value(report.bindings[2], true, "format") == field_value(report.bindings[2], false, "format"), "display pixel format is shared")) return 1;
    if (!expect(field_value(report.bindings[2], true, "buffer_policy") != field_value(report.bindings[2], false, "buffer_policy"), "display buffer policy differs by provider")) return 1;
    if (!expect(field_value(report.bindings[3], true, "source") == "null_input", "host input source is reported")) return 1;
    if (!expect(field_value(report.bindings[3], false, "source") == "h747_input_service", "h747 input source is reported")) return 1;
    if (!expect(field_value(report.bindings[3], true, "pointer") != field_value(report.bindings[3], false, "pointer"), "input pointer facts differ by provider")) return 1;
    if (!expect(field_value(report.bindings[3], true, "encoders") != field_value(report.bindings[3], false, "encoders"), "input encoder facts differ by provider")) return 1;
    if (!expect(explain_report_ok(report), "profile explain projection reports expected Phase 1 facts")) return 1;
    if (!expect(text_has_explain_facts(text), "presentation includes profile explain facts")) return 1;

    std::fwrite(text.data(), 1U, text.size(), stdout);
    std::puts("[h747-host-profile-compare-ci] ok=1");
    std::puts("[h747-host-profile-explain-ci] ok=1");
    return 0;
}
