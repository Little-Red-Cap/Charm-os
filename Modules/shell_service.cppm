module;

#include <array>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <string_view>

export module shell_service;

import util.core;
import shell_core;
import shell_cmd;
import shell_stdio;

export namespace shell_service {
    enum class JobState : util::u8 {
        running = 0,
        stopped = 1
    };

    struct Job {
        util::u32 id{0};
        std::string_view name{};
        JobState state{JobState::running};
        bool used{false};
    };

    template <util::usize MaxJobs>
    class JobTable {
    public:
        JobTable() = default;

        util::u32 start(std::string_view name) noexcept {
            for (auto& job : jobs_) {
                if (!job.used) {
                    job.used = true;
                    job.id = next_id_++;
                    job.name = name;
                    job.state = JobState::running;
                    return job.id;
                }
            }
            return 0;
        }

        bool stop(util::u32 id) noexcept {
            for (auto& job : jobs_) {
                if (job.used && job.id == id) {
                    job.state = JobState::stopped;
                    return true;
                }
            }
            return false;
        }

        void list(shell::Console& con) const noexcept {
            for (const auto& job : jobs_) {
                if (!job.used) continue;
                char buf[96]{};
                const char* state = job.state == JobState::running ? "running" : "stopped";
                std::snprintf(buf, sizeof(buf), "[%u] %s %.*s\n",
                              static_cast<unsigned>(job.id),
                              state,
                              static_cast<int>(job.name.size()),
                              job.name.data());
                (void)shell::write(con, buf);
            }
        }

    private:
        std::array<Job, MaxJobs> jobs_{};
        util::u32 next_id_{1};
    };

    inline JobTable<4>* g_jobs = nullptr;

    inline void set_job_table(JobTable<4>* table) noexcept {
        g_jobs = table;
    }

    inline shell::Result cmd_jobs(shell::Console& con, int, std::span<std::string_view>) noexcept {
        if (!g_jobs) return shell::err(shell::Errno::nosys);
        g_jobs->list(con);
        return shell::ok();
    }

    inline shell::Result cmd_start(shell::Console&, int argc, std::span<std::string_view> argv) noexcept {
        if (!g_jobs) return shell::err(shell::Errno::nosys);
        if (argc < 2) return shell::err(shell::Errno::inval);
        const auto id = g_jobs->start(argv[1]);
        return id == 0 ? shell::err(shell::Errno::busy) : shell::ok();
    }

    inline shell::Result cmd_stop(shell::Console&, int argc, std::span<std::string_view> argv) noexcept {
        if (!g_jobs) return shell::err(shell::Errno::nosys);
        if (argc < 2) return shell::err(shell::Errno::inval);
        const auto id = static_cast<util::u32>(std::strtoul(argv[1].data(), nullptr, 10));
        return g_jobs->stop(id) ? shell::ok() : shell::err(shell::Errno::noent);
    }

    struct VarsEntry {
        std::string_view key{};
        std::string_view value{};
        bool used{false};
    };

    template <util::usize MaxVars>
    class Vars {
    public:
        bool set(std::string_view key, std::string_view value) noexcept {
            auto* slot = find(key);
            if (!slot) {
                slot = free_slot();
                if (!slot) return false;
                slot->key = key;
                slot->used = true;
            }
            slot->value = value;
            return true;
        }

        std::string_view get(std::string_view key) const noexcept {
            for (const auto& e : vars_) {
                if (e.used && e.key == key) return e.value;
            }
            return {};
        }

        void list(shell::Console& con) const noexcept {
            for (const auto& e : vars_) {
                if (!e.used) continue;
                (void)shell::write(con, e.key);
                (void)shell::write(con, "=");
                (void)shell::write(con, e.value);
                (void)shell::write(con, "\n");
            }
        }

    private:
        VarsEntry* find(std::string_view key) noexcept {
            for (auto& e : vars_) {
                if (e.used && e.key == key) return &e;
            }
            return nullptr;
        }

        VarsEntry* free_slot() noexcept {
            for (auto& e : vars_) {
                if (!e.used) return &e;
            }
            return nullptr;
        }

        std::array<VarsEntry, MaxVars> vars_{};
    };

    template <util::usize MaxVars>
    inline Vars<MaxVars>* g_vars = nullptr;

    template <util::usize MaxVars>
    inline void set_vars(Vars<MaxVars>* vars) noexcept {
        g_vars<MaxVars> = vars;
    }

    template <util::usize MaxVars>
    inline shell::Result cmd_set(shell::Console&, int argc, std::span<std::string_view> argv) noexcept {
        if (!g_vars<MaxVars>) return shell::err(shell::Errno::nosys);
        if (argc < 3) return shell::err(shell::Errno::inval);
        return g_vars<MaxVars>->set(argv[1], argv[2]) ? shell::ok() : shell::err(shell::Errno::busy);
    }

    template <util::usize MaxVars>
    inline shell::Result cmd_get(shell::Console& con, int argc, std::span<std::string_view> argv) noexcept {
        if (!g_vars<MaxVars>) return shell::err(shell::Errno::nosys);
        if (argc < 2) return shell::err(shell::Errno::inval);
        const auto val = g_vars<MaxVars>->get(argv[1]);
        if (val.empty()) return shell::err(shell::Errno::noent);
        (void)shell::write(con, val);
        (void)shell::write(con, "\n");
        return shell::ok();
    }

    template <util::usize MaxVars>
    inline shell::Result cmd_vars(shell::Console& con, int, std::span<std::string_view>) noexcept {
        if (!g_vars<MaxVars>) return shell::err(shell::Errno::nosys);
        g_vars<MaxVars>->list(con);
        return shell::ok();
    }

    template <util::usize MaxAliases>
    class Alias {
    public:
        bool set(std::string_view key, std::string_view value) noexcept {
            auto* slot = find(key);
            if (!slot) {
                slot = free_slot();
                if (!slot) return false;
                slot->key = key;
                slot->used = true;
            }
            slot->value = value;
            return true;
        }

        std::string_view get(std::string_view key) const noexcept {
            for (const auto& e : aliases_) {
                if (e.used && e.key == key) return e.value;
            }
            return {};
        }

        void list(shell::Console& con) const noexcept {
            for (const auto& e : aliases_) {
                if (!e.used) continue;
                (void)shell::write(con, e.key);
                (void)shell::write(con, " -> ");
                (void)shell::write(con, e.value);
                (void)shell::write(con, "\n");
            }
        }

    private:
        VarsEntry* find(std::string_view key) noexcept {
            for (auto& e : aliases_) {
                if (e.used && e.key == key) return &e;
            }
            return nullptr;
        }

        VarsEntry* free_slot() noexcept {
            for (auto& e : aliases_) {
                if (!e.used) return &e;
            }
            return nullptr;
        }

        std::array<VarsEntry, MaxAliases> aliases_{};
    };

    template <util::usize MaxAliases>
    inline Alias<MaxAliases>* g_alias = nullptr;

    template <util::usize MaxAliases>
    inline void set_alias(Alias<MaxAliases>* alias) noexcept {
        g_alias<MaxAliases> = alias;
    }

    template <util::usize MaxAliases>
    inline shell::Result cmd_alias(shell::Console& con, int argc, std::span<std::string_view> argv) noexcept {
        if (!g_alias<MaxAliases>) return shell::err(shell::Errno::nosys);
        if (argc == 1) {
            g_alias<MaxAliases>->list(con);
            return shell::ok();
        }
        if (argc < 3) return shell::err(shell::Errno::inval);
        return g_alias<MaxAliases>->set(argv[1], argv[2]) ? shell::ok() : shell::err(shell::Errno::busy);
    }

    template <shell::Result (*Runner)(shell::Console&, std::string_view) noexcept>
    inline shell::Result run_script(shell::Console& con, std::string_view script) noexcept {
        util::usize pos = 0;
        while (pos < script.size()) {
            while (pos < script.size() && (script[pos] == '\n' || script[pos] == '\r')) ++pos;
            if (pos >= script.size()) break;
            const util::usize start = pos;
            while (pos < script.size() && script[pos] != '\n' && script[pos] != '\r') ++pos;
            auto line = script.substr(start, pos - start);
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
                line.remove_prefix(1);
            }
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
                line.remove_suffix(1);
            }
            if (line.empty() || line.front() == '#') continue;
            auto st = Runner(con, line);
                if (!st) return st;
        }
        return shell::ok();
    }
}
