#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import init.materialize;
import init.meta;
import init.observe;
import init.plan;
import init.recipe;
import init.node;
import util.core;
import util.error;

namespace {
    constexpr const char* kDefaultDotPath = "materialized_graph.dot";
    constexpr const char* kDefaultJsonPath = "materialized_graph.sample.json";

    bool write_text_file(const char* path, const char* data, std::size_t bytes) noexcept {
        if (!path || !data) {
            return false;
        }
        std::FILE* file = std::fopen(path, "wb");
        if (!file) {
            return false;
        }
        const auto written = std::fwrite(data, 1, bytes, file);
        std::fclose(file);
        return written == bytes;
    }

    void print_usage() noexcept {
        std::printf("usage: init-materialize-observe-demo [--dot PATH] [--json PATH]\n");
        std::printf("default dot : %s\n", kDefaultDotPath);
        std::printf("default json: %s\n", kDefaultJsonPath);
    }

    struct DemoContext {
        util::u32 boot_count{1};
    };

    inline util::Result<void> start_clock(DemoContext& ctx) noexcept {
        ctx.boot_count += 1;
        return {};
    }

    inline util::Result<void> start_console(DemoContext& ctx) noexcept {
        ctx.boot_count += 1;
        return {};
    }

    using CapClock = init::cap_c<"demo.clock">;
    using CapConsole = init::cap_c<"demo.console">;
    using CapLegacy = init::cap_c<"demo.legacy">;
    using CapReady = init::cap_c<"demo.ready">;

    using ClockRecipe = init::recipe_desc<
        "demo.clock.init",
        init::Phase::early,
        static_cast<util::u32>(init::Runlevel::all),
        init::cap_list<CapClock>,
        init::cap_list<>,
        DemoContext,
        &start_clock>;

    using ConsoleRecipe = init::recipe_desc<
        "demo.console.init",
        init::Phase::service,
        static_cast<util::u32>(init::Runlevel::full),
        init::cap_list<CapConsole>,
        init::cap_list<CapClock>,
        DemoContext,
        &start_console>;

    struct LegacyTelemetryNode {
        std::array<init::CapId, 1> provides{CapLegacy::id};
        std::array<init::CapId, 1> required_caps{CapClock::id};

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            if (id == CapLegacy::id) {
                return CapLegacy::view();
            }
            if (id == CapClock::id) {
                return CapClock::view();
            }
            return {};
        }

        LegacyTelemetryNode() noexcept
            : node{
                "demo.legacy.telemetry",
                init::Phase::service,
                static_cast<util::u32>(init::Runlevel::full),
                std::span<const init::CapId>{provides.data(), provides.size()},
                std::span<const init::CapId>{required_caps.data(), required_caps.size()},
                nullptr,
                nullptr,
                this
            } {
        }

        init::Node node{};
    };

    template <typename Plan>
    int export_demo(const Plan& plan_value,
                    const char* dot_path,
                    const char* json_path) noexcept {
        auto mats = init::materialize<8, 16>(plan_value);
        if (!mats) {
            std::printf("[observe-demo] materialize failed err=%d\n", static_cast<int>(mats.error()));
            return 1;
        }

        auto view = init::observe(*mats);
        std::array<char, 8192> dot{};
        std::array<char, 8192> json{};

        const auto dot_bytes = init::format_dot(*mats, dot.data(), dot.size());
        const auto json_bytes = init::format_json_sample(*mats, json.data(), json.size());
        if (dot_bytes == 0 || json_bytes == 0) {
            std::printf("[observe-demo] export formatting failed\n");
            return 1;
        }
        if (!write_text_file(dot_path, dot.data(), dot_bytes)) {
            std::printf("[observe-demo] write failed path=%s\n", dot_path);
            return 1;
        }
        if (!write_text_file(json_path, json.data(), json_bytes)) {
            std::printf("[observe-demo] write failed path=%s\n", json_path);
            return 1;
        }

        std::printf("[observe-demo] nodes=%llu edges=%llu\n",
                    static_cast<unsigned long long>(view.node_count),
                    static_cast<unsigned long long>(view.edge_count));
        std::printf("[observe-demo] dot=%s bytes=%llu\n",
                    dot_path,
                    static_cast<unsigned long long>(dot_bytes));
        std::printf("[observe-demo] json=%s bytes=%llu\n",
                    json_path,
                    static_cast<unsigned long long>(json_bytes));
        return 0;
    }
}

int main(int argc, char** argv) {
    const char* dot_path = kDefaultDotPath;
    const char* json_path = kDefaultJsonPath;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dot") == 0) {
            if (i + 1 >= argc) {
                print_usage();
                return 1;
            }
            dot_path = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--json") == 0) {
            if (i + 1 >= argc) {
                print_usage();
                return 1;
            }
            json_path = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }

        std::printf("unknown option: %s\n", argv[i]);
        print_usage();
        return 1;
    }

    DemoContext ctx{};
    LegacyTelemetryNode legacy{};
    const auto plan_value = init::compose(
        init::bind<ClockRecipe>(ctx),
        init::bind<ConsoleRecipe>(ctx),
        init::as_plan(legacy)).ready_as<CapReady>();

    return export_demo(plan_value, dot_path, json_path);
}
