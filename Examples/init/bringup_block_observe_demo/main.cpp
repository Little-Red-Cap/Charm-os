#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <array>
#include <cstdio>
#include <cstring>

import block.registry;
import charm.system.bringup.block;
import charm.system.init_block;
import charm.system.reactor_pump;
import init.materialize;
import init.node;
import init.observe;
import kernel.eda;
import kernel.evt;
import platform.board.win_stub;
import util.core;

namespace {
    constexpr const char* kDefaultDotPath = "bringup_block_materialized_graph.dot";
    constexpr const char* kDefaultJsonPath = "bringup_block_materialized_graph.sample.json";
    constexpr const char* kDefaultImagePath = "observe-block.img";

    bool noop_post(void*, kernel::TaskId, kernel::Event) noexcept {
        return true;
    }

    struct ExportHost {
        charm::system::ReactorPumpTask pump_task{};

        charm::system::ReactorPumpTask& pump() noexcept {
            return pump_task;
        }

        charm::system::PostFn post_io_ready_fn() noexcept {
            return &noop_post;
        }

        charm::system::PostFn post_demand_fn() noexcept {
            return &noop_post;
        }

        void* post_ctx() noexcept {
            return nullptr;
        }

        static constexpr kernel::TaskId pump_id() noexcept {
            return {};
        }
    };

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
        std::printf("usage: init-bringup-block-observe-demo [--dot PATH] [--json PATH] [--image PATH]\n");
        std::printf("default dot  : %s\n", kDefaultDotPath);
        std::printf("default json : %s\n", kDefaultJsonPath);
        std::printf("default image: %s\n", kDefaultImagePath);
    }
}

int main(int argc, char** argv) {
    const char* dot_path = kDefaultDotPath;
    const char* json_path = kDefaultJsonPath;
    const char* image_path = kDefaultImagePath;

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
        if (std::strcmp(argv[i], "--image") == 0) {
            if (i + 1 >= argc) {
                print_usage();
                return 1;
            }
            image_path = argv[++i];
            continue;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        std::printf("unexpected arg: %s\n", argv[i]);
        print_usage();
        return 1;
    }

    auto caps = platform::board::win_stub::make_block_caps();
    ExportHost host{};
    charm::system::BringupBlock<8, 16, 8> bringup{caps, host};

    charm::system::FileInitChain<block::Registry<8>> file_chain{
        bringup.block_registry(),
        image_path,
        512,
        "block.sd0"
    };

    const auto bringup_plan = bringup.plan(
        file_chain.plan(),
        static_cast<util::u32>(init::Runlevel::all),
        init::Phase::app);

    auto mats = init::materialize<16, 32>(bringup_plan);
    if (!mats) {
        std::printf("materialize failed err=%d\n", static_cast<int>(mats.error()));
        return 1;
    }

    std::array<char, 8192> dot{};
    const auto dot_bytes = init::format_dot(*mats, dot.data(), dot.size());
    if (dot_bytes == 0 || !write_text_file(dot_path, dot.data(), dot_bytes)) {
        std::printf("write dot failed: %s\n", dot_path);
        return 1;
    }

    std::array<char, 8192> json{};
    const auto json_bytes = init::format_json_sample(*mats, json.data(), json.size());
    if (json_bytes == 0 || !write_text_file(json_path, json.data(), json_bytes)) {
        std::printf("write json failed: %s\n", json_path);
        return 1;
    }

    std::printf("[OK] dot exported : %s (%zu bytes)\n", dot_path, dot_bytes);
    std::printf("[OK] json exported: %s (%zu bytes)\n", json_path, json_bytes);
    return 0;
}
