module;

#include <array>
#include <cstddef>
#include <cstring>
#include <expected>
#include <string_view>
#include <utility>

export module kernel.scheduler_export;

import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import kernel.ssu;
import kernel.task_state;
import out.core;
import out.format;
import out.sink;
import util.core;

namespace kernel::scheduler_export_detail {
    [[nodiscard]] inline std::string_view to_text(kernel::ssu::ExecutionDomain v) noexcept {
        switch (v) {
            case kernel::ssu::ExecutionDomain::isr_only: return "isr_only";
            case kernel::ssu::ExecutionDomain::task_only: return "task_only";
            case kernel::ssu::ExecutionDomain::anywhere: return "anywhere";
        }
        return "unknown";
    }

    [[nodiscard]] inline std::string_view to_text(kernel::ssu::TriggerKind v) noexcept {
        switch (v) {
            case kernel::ssu::TriggerKind::event: return "event";
            case kernel::ssu::TriggerKind::io_ready: return "io_ready";
            case kernel::ssu::TriggerKind::timer: return "timer";
            case kernel::ssu::TriggerKind::frame: return "frame";
            case kernel::ssu::TriggerKind::demand: return "demand";
        }
        return "unknown";
    }

    [[nodiscard]] inline std::string_view to_text(kernel::ssu::BudgetKind v) noexcept {
        switch (v) {
            case kernel::ssu::BudgetKind::single_step: return "single_step";
            case kernel::ssu::BudgetKind::budgeted: return "budgeted";
        }
        return "unknown";
    }

    [[nodiscard]] inline std::string_view to_text(kernel::ssu::BlockingKind v) noexcept {
        switch (v) {
            case kernel::ssu::BlockingKind::non_blocking: return "non_blocking";
            case kernel::ssu::BlockingKind::may_block: return "may_block";
        }
        return "unknown";
    }

    struct trunc_sink {
        char* buf{nullptr};
        std::size_t cap{0};
        std::size_t pos{0};

        out::result<std::size_t> write(out::bytes b) noexcept {
            if (!buf || cap == 0) return std::unexpected(out::errc::buffer_overflow);
            const std::size_t avail = (pos < cap) ? (cap - pos) : 0;
            const std::size_t n = (b.size() < avail) ? b.size() : avail;
            if (n > 0) {
                std::memcpy(buf + pos, b.data(), n);
                pos += n;
            }
            if (n < b.size()) return std::unexpected(out::errc::buffer_overflow);
            return out::ok(b.size());
        }
    };

    inline std::size_t append_text(char* out, std::size_t max, std::size_t offset,
                                   std::string_view sv) noexcept {
        if (!out || max == 0 || offset >= max) return offset;
        trunc_sink sink{out + offset, max - offset - 1u, 0u};
        const auto b = out::bytes{reinterpret_cast<const std::byte*>(sv.data()), sv.size()};
        (void)sink.write(b);
        const std::size_t n = sink.pos;
        out[offset + n] = '\0';
        return offset + n;
    }

    template <out::fixed_string Fmt, class... Args>
    inline std::size_t append_fmt(char* out, std::size_t max, std::size_t offset, Args&&... args) noexcept {
        if (!out || max == 0 || offset >= max) return offset;
        trunc_sink sink{out + offset, max - offset - 1u, 0u};
        (void)out::vprint<Fmt>(sink, std::forward<Args>(args)...);
        const std::size_t n = sink.pos;
        out[offset + n] = '\0';
        return offset + n;
    }
}

export namespace kernel {
    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_snapshot_json(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        const auto snap = scheduler.snapshot();
        std::size_t offset = 0;
        offset = scheduler_export_detail::append_text(out, max, offset, "{");
        offset = scheduler_export_detail::append_fmt<
            "\"posted\":{},\"dropped\":{},\"dispatched\":{},\"filtered\":{},\"filtered_run\":{},\"budget\":{},"
            "\"maxQ\":{},\"maxT\":{},\"queue\":{},\"timers\":{},\"active\":{},"
            "\"dedup\":{},\"debounce\":{},\"coalesce\":{},\"idle\":{}">(
            out, max, offset,
            static_cast<unsigned long long>(snap.stats.posted),
            static_cast<unsigned long long>(snap.stats.dropped),
            static_cast<unsigned long long>(snap.stats.dispatched),
            static_cast<unsigned long long>(snap.stats.filtered),
            static_cast<unsigned long long>(snap.stats.filtered_run),
            static_cast<unsigned long long>(snap.stats.budget_limited),
            static_cast<unsigned long long>(snap.stats.max_queue),
            static_cast<unsigned long long>(snap.stats.max_timer),
            static_cast<unsigned long long>(snap.queue_depth),
            static_cast<unsigned long long>(snap.timer_depth),
            static_cast<unsigned long long>(snap.active_tasks),
            static_cast<unsigned long long>(snap.stats.dedup_filtered),
            static_cast<unsigned long long>(snap.stats.debounce_filtered),
            static_cast<unsigned long long>(snap.stats.coalesce_hit),
            static_cast<unsigned long long>(snap.stats.idle_rounds));
        offset = scheduler_export_detail::append_text(out, max, offset, "}");
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_event_stats_json(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        const auto& stats = scheduler.stats();
        std::size_t offset = 0;
        offset = scheduler_export_detail::append_text(out, max, offset, "[");
        for (std::size_t i = 0; i < event_id_count; ++i) {
            if (i > 0) {
                offset = scheduler_export_detail::append_text(out, max, offset, ",");
            }
            offset = scheduler_export_detail::append_text(out, max, offset, "{");
            offset = scheduler_export_detail::append_fmt<"\"id\":{},\"posted\":{},\"dispatched\":{}">(
                out, max, offset,
                static_cast<unsigned long long>(i),
                static_cast<unsigned long long>(stats.event_posted[i]),
                static_cast<unsigned long long>(stats.event_dispatched[i]));
            offset = scheduler_export_detail::append_text(out, max, offset, "}");
        }
        offset = scheduler_export_detail::append_text(out, max, offset, "]");
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_event_source_json(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        const auto& stats = scheduler.stats();
        std::size_t offset = 0;
        offset = scheduler_export_detail::append_text(out, max, offset, "{");
        offset = scheduler_export_detail::append_fmt<"\"post\":{},\"io_ready\":{},\"demand\":{},\"timer\":{},\"replay\":{}">(
            out, max, offset,
            static_cast<unsigned long long>(stats.source_post),
            static_cast<unsigned long long>(stats.source_io_ready),
            static_cast<unsigned long long>(stats.source_demand),
            static_cast<unsigned long long>(stats.source_timer),
            static_cast<unsigned long long>(stats.source_replay));
        offset = scheduler_export_detail::append_text(out, max, offset, "}");
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_ssu_overview_json(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        std::array<util::u64, 5> trigger{};
        std::array<util::u64, 2> budget{};
        std::array<util::u64, 2> blocking{};
        std::array<util::u64, 3> domain{};
        util::u64 unnamed{0};

        const auto tasks = scheduler.task_snapshot();
        for (const auto& task : tasks) {
            ++trigger[static_cast<std::size_t>(task.ssu_trigger)];
            ++budget[static_cast<std::size_t>(task.ssu_budget)];
            ++blocking[static_cast<std::size_t>(task.ssu_blocking)];
            ++domain[static_cast<std::size_t>(task.ssu_domain)];
            if (task.ssu_name.empty()) {
                ++unnamed;
            }
        }

        std::size_t offset = 0;
        offset = scheduler_export_detail::append_text(out, max, offset, R"({"tasks":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(tasks.size()));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"unnamed":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(unnamed));

        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"trigger":{"event":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(trigger[0]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"io_ready":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(trigger[1]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"timer":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(trigger[2]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"frame":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(trigger[3]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"demand":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(trigger[4]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(})");

        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"budget":{"single_step":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(budget[0]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"budgeted":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(budget[1]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(})");

        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"blocking":{"non_blocking":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(blocking[0]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"may_block":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(blocking[1]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(})");

        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"domain":{"isr_only":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(domain[0]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"task_only":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(domain[1]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"anywhere":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(domain[2]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(}})");
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_ssu_hotspots_json(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        const auto& stats = scheduler.stats();
        const std::array<util::u64, 5> submit_counts{
            stats.source_post,
            stats.source_io_ready,
            stats.source_demand,
            stats.source_timer,
            stats.source_replay,
        };
        const std::array<std::string_view, 5> submit_names{
            "post",
            "io_ready",
            "demand",
            "timer",
            "replay",
        };

        std::size_t dominant = 0;
        for (std::size_t i = 1; i < submit_counts.size(); ++i) {
            if (submit_counts[i] > submit_counts[dominant]) {
                dominant = i;
            }
        }

        const auto tasks = scheduler.task_snapshot();
        std::array<TaskId, tasks.size()> unnamed{};
        std::size_t unnamed_count = 0;
        for (const auto& task : tasks) {
            if (task.ssu_name.empty() && unnamed_count < unnamed.size()) {
                unnamed[unnamed_count++] = task.id;
            }
        }

        const util::u64 submit_total = submit_counts[0] + submit_counts[1] + submit_counts[2] + submit_counts[3] + submit_counts[4];
        const util::u64 demand_share_permille = submit_total == 0 ? 0 : (submit_counts[2] * 1000ull) / submit_total;
        const auto kDemandWarnPermille = static_cast<util::u64>(Config::ssu_demand_warn_permille);
        const auto kDemandErrorPermille = static_cast<util::u64>(Config::ssu_demand_err_permille);

        const bool no_demand_submit = submit_counts[2] == 0;
        const bool low_demand_share_warn = submit_total > 0 && demand_share_permille < kDemandWarnPermille;
        const bool low_demand_share_error = submit_total > 0 && demand_share_permille < kDemandErrorPermille;
        const bool has_unnamed_tasks = unnamed_count > 0;

        std::string_view risk_level = "ok";
        if (has_unnamed_tasks || no_demand_submit || low_demand_share_error) {
            risk_level = "error";
        } else if (low_demand_share_warn) {
            risk_level = "warning";
        }

        std::size_t offset = 0;
        offset = scheduler_export_detail::append_text(out, max, offset, R"({"dominant_submit":")");
        offset = scheduler_export_detail::append_text(out, max, offset, submit_names[dominant]);
        offset = scheduler_export_detail::append_text(out, max, offset, R"(","submit":{"post":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(submit_counts[0]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"io_ready":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(submit_counts[1]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"demand":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(submit_counts[2]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"timer":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(submit_counts[3]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"replay":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(submit_counts[4]));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(},"submit_total":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(submit_total));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"demand_share_permille":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(demand_share_permille));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"unnamed_count":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(unnamed_count));
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"risk_level":")");
        offset = scheduler_export_detail::append_text(out, max, offset, risk_level);
        offset = scheduler_export_detail::append_text(out, max, offset, R"(","risk":{"unnamed_tasks":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, has_unnamed_tasks ? 1u : 0u);
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"no_demand_submit":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, no_demand_submit ? 1u : 0u);
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"low_demand_share_warn":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, low_demand_share_warn ? 1u : 0u);
        offset = scheduler_export_detail::append_text(out, max, offset, R"(,"low_demand_share_error":)");
        offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, low_demand_share_error ? 1u : 0u);
        offset = scheduler_export_detail::append_text(out, max, offset, R"(},"unnamed_tasks":[)");

        for (std::size_t i = 0; i < unnamed_count; ++i) {
            if (i > 0) {
                offset = scheduler_export_detail::append_text(out, max, offset, ",");
            }
            offset = scheduler_export_detail::append_fmt<"{}">(out, max, offset, static_cast<unsigned long long>(unnamed[i].value));
        }
        offset = scheduler_export_detail::append_text(out, max, offset, R"(]})");
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_tasks_json(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        std::size_t offset = 0;
        offset = scheduler_export_detail::append_text(out, max, offset, "[");
        auto tasks = scheduler.task_snapshot();
        for (std::size_t i = 0; i < tasks.size(); ++i) {
            const auto& t = tasks[i];
            if (i > 0) {
                offset = scheduler_export_detail::append_text(out, max, offset, ",");
            }
            offset = scheduler_export_detail::append_text(out, max, offset, "{");
            offset = scheduler_export_detail::append_fmt<"\"id\":{},\"state\":{},\"enabled\":{},\"prio\":{},\"active\":{}">(
                out, max, offset,
                static_cast<unsigned long long>(t.id.value),
                static_cast<unsigned>(t.state),
                t.enabled ? 1u : 0u,
                static_cast<unsigned>(t.priority),
                t.active ? 1u : 0u);
            if (!t.ssu_name.empty()) {
                offset = scheduler_export_detail::append_fmt<",\"ssu\":\"{}\"">(out, max, offset, t.ssu_name);
                offset = scheduler_export_detail::append_fmt<",\"ssu_domain\":\"{}\",\"ssu_trigger\":\"{}\",\"ssu_budget\":\"{}\",\"ssu_blocking\":\"{}\"">(
                    out,
                    max,
                    offset,
                    scheduler_export_detail::to_text(t.ssu_domain),
                    scheduler_export_detail::to_text(t.ssu_trigger),
                    scheduler_export_detail::to_text(t.ssu_budget),
                    scheduler_export_detail::to_text(t.ssu_blocking));
            }
            offset = scheduler_export_detail::append_text(out, max, offset, "}");
        }
        offset = scheduler_export_detail::append_text(out, max, offset, "]");
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_snapshot_diff(
        Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        const auto diff = scheduler.snapshot_diff();
        std::size_t offset = 0;
        offset = scheduler_export_detail::append_fmt<
            "posted={} dropped={} dispatched={} filtered={} filtered_run={} budget={}">(
            out, max, offset,
            static_cast<long long>(diff.stats.posted),
            static_cast<long long>(diff.stats.dropped),
            static_cast<long long>(diff.stats.dispatched),
            static_cast<long long>(diff.stats.filtered),
            static_cast<long long>(diff.stats.filtered_run),
            static_cast<long long>(diff.stats.budget_limited));
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_trace_json(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        const auto count = scheduler.trace_snapshot_size();
        if (count == 0) {
            return scheduler_export_detail::append_text(out, max, 0, "[]");
        }

        const auto trace = scheduler.trace_snapshot();
        std::size_t offset = 0;
        offset = scheduler_export_detail::append_text(out, max, offset, "[");
        for (std::size_t i = 0; i < count; ++i) {
            const auto& rec = trace[i].record;
            const auto& meta = trace[i].ssu;
            if (i > 0) {
                offset = scheduler_export_detail::append_text(out, max, offset, ",");
            }
            offset = scheduler_export_detail::append_text(out, max, offset, "{");
            offset = scheduler_export_detail::append_fmt<
                "\\\"t\\\":{},\\\"task\\\":{},\\\"id\\\":{},\\\"payload\\\":{},\\\"count\\\":{},\\\"kind\\\":{}">(
                out, max, offset,
                static_cast<unsigned long long>(rec.time),
                static_cast<unsigned long long>(rec.task.value),
                static_cast<unsigned>(rec.id),
                static_cast<unsigned long long>(rec.payload),
                static_cast<unsigned>(rec.count),
                static_cast<unsigned>(rec.kind));
            if (!meta.name.empty()) {
                offset = scheduler_export_detail::append_fmt<",\\\"ssu\\\":\\\"{}\\\",\\\"ssu_domain\\\":\\\"{}\\\",\\\"ssu_trigger\\\":\\\"{}\\\",\\\"ssu_budget\\\":\\\"{}\\\",\\\"ssu_blocking\\\":\\\"{}\\\"">(
                    out,
                    max,
                    offset,
                    meta.name,
                    scheduler_export_detail::to_text(meta.domain),
                    scheduler_export_detail::to_text(meta.trigger),
                    scheduler_export_detail::to_text(meta.budget),
                    scheduler_export_detail::to_text(meta.blocking));
            }
            offset = scheduler_export_detail::append_text(out, max, offset, "}");
        }
        offset = scheduler_export_detail::append_text(out, max, offset, "]");
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_trace_csv(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        std::size_t offset = 0;
        offset = scheduler_export_detail::append_text(out, max, offset, "trace_v1,t,task,id,payload,count,kind,ssu,ssu_domain,ssu_trigger,ssu_budget,ssu_blocking\n");
        const auto count = scheduler.trace_snapshot_size();
        if (count == 0) {
            return offset;
        }

        const auto trace = scheduler.trace_snapshot();
        for (std::size_t i = 0; i < count; ++i) {
            const auto& rec = trace[i].record;
            const auto& meta = trace[i].ssu;
            offset = scheduler_export_detail::append_fmt<"{},{},{},{},{},{},{},{},{},{},{}\n">(
                out, max, offset,
                static_cast<unsigned long long>(rec.time),
                static_cast<unsigned long long>(rec.task.value),
                static_cast<unsigned>(rec.id),
                static_cast<unsigned long long>(rec.payload),
                static_cast<unsigned>(rec.count),
                static_cast<unsigned>(rec.kind),
                meta.name,
                scheduler_export_detail::to_text(meta.domain),
                scheduler_export_detail::to_text(meta.trigger),
                scheduler_export_detail::to_text(meta.budget),
                scheduler_export_detail::to_text(meta.blocking));
            if (offset >= max) {
                break;
            }
        }
        return offset;
    }

    template <typename Config, typename Registry, typename Caps>
    [[nodiscard]] std::size_t format_snapshot(
        const Scheduler<Config, Registry, Caps, state::Running>& scheduler,
        char* out,
        std::size_t max) noexcept {
        const auto snap = scheduler.snapshot();
        std::size_t offset = 0;
        offset = scheduler_export_detail::append_fmt<
            "posted={} dropped={} dispatched={} filtered={} filtered_run={} budget={} maxQ={} maxT={} queue={} "
            "timers={} active={} dedup={} debounce={} coalesce={} idle={}">(
            out, max, offset,
            static_cast<unsigned long long>(snap.stats.posted),
            static_cast<unsigned long long>(snap.stats.dropped),
            static_cast<unsigned long long>(snap.stats.dispatched),
            static_cast<unsigned long long>(snap.stats.filtered),
            static_cast<unsigned long long>(snap.stats.filtered_run),
            static_cast<unsigned long long>(snap.stats.budget_limited),
            static_cast<unsigned long long>(snap.stats.max_queue),
            static_cast<unsigned long long>(snap.stats.max_timer),
            static_cast<unsigned long long>(snap.queue_depth),
            static_cast<unsigned long long>(snap.timer_depth),
            static_cast<unsigned long long>(snap.active_tasks),
            static_cast<unsigned long long>(snap.stats.dedup_filtered),
            static_cast<unsigned long long>(snap.stats.debounce_filtered),
            static_cast<unsigned long long>(snap.stats.coalesce_hit),
            static_cast<unsigned long long>(snap.stats.idle_rounds));
        return offset;
    }
}
