export module power.port;

import power.types;

export namespace power {
    struct PortOps {
        bool (*enter)(State state) noexcept { nullptr };
        void (*exit)(State state) noexcept { nullptr };
    };
}
