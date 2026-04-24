#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <array>
#include <cstdio>
#include <cstring>

import charm.system.bringup;
import charm.system.clock;
import charm.system.reactor_pump;
import init.materialize;
import init.observe;
import input.pump;
import kernel.eda;
import kernel.evt;
import platform.board.win_stub;

namespace {
    constexpr const char* kDefaultDotPath = "bringup_minimal_materialized_graph.dot";
    constexpr const char* kDefaultJsonPath = "bringup_minimal_materialized_graph.sample.json";

    bool noop_post(void*, kernel::TaskId, kernel::Event) noexcept {
        return true;
    }

    bool noop_schedule(void*, kernel::TaskId, kernel::Event, charm::system::ClockTick) noexcept {
        return true;
    }

    struct ExportHost {
        charm::system::ReactorPumpTask pump_task{};
        input::InputPumpTask input_pump_task{};

        charm::system::ReactorPumpTask& pump() noexcept {
            return pump_task;
        }

        input::InputPumpTask& input_pump() noexcept {
            return input_pump_task;
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

        input::ScheduleFn schedule_fn() noexcept {
            return &noop_schedule;
        }

        void* schedule_ctx() noexcept {
            return nullptr;
        }

        static constexpr kernel::TaskId pump_id() noexcept {
            return {};
        }

        static constexpr kernel::TaskId input_pump_id() noexcept {
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
        std::printf("usage: init-bringup-minimal-observe-demo [--dot PATH] [--json PATH]\n");
        std::printf("default dot : %s\n", kDefaultDotPath);
        std::printf("default json: %s\n", kDefaultJsonPath);
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
        std::printf("unexpected arg: %s\n", argv[i]);
        print_usage();
        return 1;
    }

    auto caps = platform::board::win_stub::make_board_caps();
    ExportHost host{};
    charm::system::BringupMinimal<8, 16, 8, 64, 64> bringup{caps, host};

    auto mats = init::materialize<32, 64>(bringup.plan());
    if (!mats) {
        std::printf("materialize failed err=%d\n", static_cast<int>(mats.error()));
        return 1;
    }

    std::array<char, 16384> dot{};
    const auto dot_bytes = init::format_dot(*mats, dot.data(), dot.size());
    if (dot_bytes == 0 || !write_text_file(dot_path, dot.data(), dot_bytes)) {
        std::printf("write dot failed: %s\n", dot_path);
        return 1;
    }

    std::array<char, 16384> json{};
    const auto json_bytes = init::format_json_sample(*mats, json.data(), json.size());
    if (json_bytes == 0 || !write_text_file(json_path, json.data(), json_bytes)) {
        std::printf("write json failed: %s\n", json_path);
        return 1;
    }

    std::printf("[OK] dot exported : %s (%zu bytes)\n", dot_path, dot_bytes);
    std::printf("[OK] json exported: %s (%zu bytes)\n", json_path, json_bytes);
    return 0;
}
