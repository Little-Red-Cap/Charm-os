#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

import init.connection;
import init.materialize;
import init.meta;
import init.node;
import init.observe;
import init.plan;
import init.recipe;
import util.core;
import util.error;

namespace {
    constexpr const char* kDefaultDotPath = "connection_graph.dot";
    constexpr const char* kDefaultJsonPath = "connection_graph.sample.json";

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
        std::printf("usage: init-connection-observe-demo [--dot PATH] [--json PATH]\n");
        std::printf("default dot : %s\n", kDefaultDotPath);
        std::printf("default json: %s\n", kDefaultJsonPath);
    }

    struct DemoContext {
        util::u32 boot_count{0};
    };

    inline util::Result<void> start_encoder(DemoContext& ctx) noexcept {
        ctx.boot_count += 1;
        return {};
    }

    inline util::Result<void> start_volume(DemoContext& ctx) noexcept {
        ctx.boot_count += 1;
        return {};
    }

    inline util::Result<void> start_status(DemoContext& ctx) noexcept {
        ctx.boot_count += 1;
        return {};
    }

    using CapEncoderRotate = init::cap_c<"demo.input.encoder.rotate">;
    using CapVolumeAdjust = init::cap_c<"demo.player.volume.adjust">;
    using CapStatusRefresh = init::cap_c<"demo.ui.status.refresh">;

    using EncoderRecipe = init::recipe_desc<
        "demo.input.encoder",
        init::Phase::service,
        static_cast<util::u32>(init::Runlevel::full),
        init::cap_list<CapEncoderRotate>,
        init::cap_list<>,
        DemoContext,
        &start_encoder>;

    using VolumeRecipe = init::recipe_desc<
        "demo.player.volume",
        init::Phase::service,
        static_cast<util::u32>(init::Runlevel::full),
        init::cap_list<CapVolumeAdjust>,
        init::cap_list<>,
        DemoContext,
        &start_volume>;

    using StatusRecipe = init::recipe_desc<
        "demo.ui.status",
        init::Phase::app,
        static_cast<util::u32>(init::Runlevel::full),
        init::cap_list<CapStatusRefresh>,
        init::cap_list<>,
        DemoContext,
        &start_status>;

    template <typename Plan>
    int export_demo(const Plan& plan_value, const char* dot_path, const char* json_path) noexcept {
        auto mats = init::materialize<8, 16>(plan_value);
        if (!mats) {
            std::printf("[connection-observe-demo] materialize failed err=%d\n",
                        static_cast<int>(mats.error()));
            return 1;
        }

        auto view = init::observe(*mats);
        util::usize connection_count = 0;
        util::usize direct_count = 0;
        util::usize deferred_count = 0;
        bool saw_volume_direct = false;
        bool saw_status_deferred = false;
        for (util::usize i = 0; i < view.node_count; ++i) {
            const auto& node = view.nodes[i];
            if (node.kind != init::materialized_node_kind::connection) {
                continue;
            }
            ++connection_count;
            if (node.connection_mode == std::string_view{"direct"}) {
                ++direct_count;
            } else if (node.connection_mode == std::string_view{"deferred"}) {
                ++deferred_count;
            }
            if (node.name == std::string_view{"demo.connection.volume_direct"}) {
                saw_volume_direct = node.connection_source == CapEncoderRotate::view()
                                    && node.connection_sink == CapVolumeAdjust::view()
                                    && node.connection_mode == std::string_view{"direct"};
            } else if (node.name == std::string_view{"demo.connection.status_deferred"}) {
                saw_status_deferred = node.connection_source == CapEncoderRotate::view()
                                      && node.connection_sink == CapStatusRefresh::view()
                                      && node.connection_mode == std::string_view{"deferred"};
            }
        }
        if (view.node_count != 5 || view.edge_count != 4 || connection_count != 2
            || direct_count != 1 || deferred_count != 1 || !saw_volume_direct || !saw_status_deferred) {
            std::printf("[connection-observe-demo] unexpected graph nodes=%llu edges=%llu connections=%llu direct=%llu deferred=%llu volume_direct=%d status_deferred=%d\n",
                        static_cast<unsigned long long>(view.node_count),
                        static_cast<unsigned long long>(view.edge_count),
                        static_cast<unsigned long long>(connection_count),
                        static_cast<unsigned long long>(direct_count),
                        static_cast<unsigned long long>(deferred_count),
                        saw_volume_direct ? 1 : 0,
                        saw_status_deferred ? 1 : 0);
            return 1;
        }

        std::array<char, 8192> dot{};
        std::array<char, 8192> json{};
        const auto dot_bytes = init::format_dot(view, dot.data(), dot.size());
        const auto json_bytes = init::format_json_sample(view, json.data(), json.size());
        if (dot_bytes == 0 || json_bytes == 0) {
            std::printf("[connection-observe-demo] export formatting failed\n");
            return 1;
        }
        if (std::strstr(dot.data(), "shape=hexagon") == nullptr
            || std::strstr(dot.data(), "mode=direct") == nullptr
            || std::strstr(dot.data(), "mode=deferred") == nullptr
            || std::strstr(dot.data(), "demo.connection.volume_direct") == nullptr
            || std::strstr(dot.data(), "demo.connection.status_deferred") == nullptr) {
            std::printf("[connection-observe-demo] dot export missing connection markers\n");
            return 1;
        }
        if (std::strstr(json.data(), "\"kind\":\"connection\"") == nullptr
            || std::strstr(json.data(), "\"mode\":\"direct\"") == nullptr
            || std::strstr(json.data(), "\"mode\":\"deferred\"") == nullptr
            || std::strstr(json.data(), "\"name\":\"demo.connection.volume_direct\"") == nullptr
            || std::strstr(json.data(), "\"name\":\"demo.connection.status_deferred\"") == nullptr) {
            std::printf("[connection-observe-demo] json export missing connection markers\n");
            return 1;
        }
        if (!write_text_file(dot_path, dot.data(), dot_bytes)) {
            std::printf("[connection-observe-demo] write failed path=%s\n", dot_path);
            return 1;
        }
        if (!write_text_file(json_path, json.data(), json_bytes)) {
            std::printf("[connection-observe-demo] write failed path=%s\n", json_path);
            return 1;
        }

        std::printf("[connection-observe-demo] nodes=%llu edges=%llu direct=%llu deferred=%llu\n",
                    static_cast<unsigned long long>(view.node_count),
                    static_cast<unsigned long long>(view.edge_count),
                    static_cast<unsigned long long>(direct_count),
                    static_cast<unsigned long long>(deferred_count));
        std::printf("[connection-observe-demo] dot=%s bytes=%llu\n",
                    dot_path,
                    static_cast<unsigned long long>(dot_bytes));
        std::printf("[connection-observe-demo] json=%s bytes=%llu\n",
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
    auto direct = init::direct_connection<
        "demo.connection.volume_direct",
        CapEncoderRotate,
        CapVolumeAdjust>();
    auto deferred = init::deferred_connection<
        "demo.connection.status_deferred",
        CapEncoderRotate,
        CapStatusRefresh,
        init::Phase::app>();

    const auto plan_value = init::compose(
        init::bind<EncoderRecipe>(ctx),
        init::bind<VolumeRecipe>(ctx),
        init::bind<StatusRecipe>(ctx),
        init::as_plan(direct),
        init::as_plan(deferred));

    return export_demo(plan_value, dot_path, json_path);
}
