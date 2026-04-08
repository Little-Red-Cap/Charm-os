module;

#include <csetjmp>

export module posix.exec_context;

export namespace posix {
    struct ExecContext {
        void* owner{nullptr};
        int pid_value{-1};
        int errno_value{0};
        bool exit_requested{false};
        int exit_code{0};
        bool jump_ready{false};
        std::jmp_buf* exit_jmp{nullptr};
        ExecContext* previous{nullptr};
    };

    struct ExecJumpBuffer {
        std::jmp_buf storage{};
    };

    inline ExecContext*& active_exec_context() noexcept {
        static ExecContext* current = nullptr;
        return current;
    }

    inline void push_exec_context(ExecContext& ctx) noexcept {
        ctx.previous = active_exec_context();
        active_exec_context() = &ctx;
    }

    inline void pop_exec_context(ExecContext& ctx) noexcept {
        active_exec_context() = ctx.previous;
        ctx.previous = nullptr;
    }

    inline int finalize_exec_exit(const ExecContext& ctx, int rc) noexcept {
        return ctx.exit_requested ? ctx.exit_code : rc;
    }

    inline void request_exec_exit(ExecContext& ctx, int code) noexcept {
        ctx.exit_requested = true;
        ctx.exit_code = code;
        if (ctx.jump_ready && ctx.exit_jmp) {
            std::longjmp(*ctx.exit_jmp, 1);
        }
    }
}