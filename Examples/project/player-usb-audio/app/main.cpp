#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

import usb.class_cdc;
import usb.common;
import usb.device_driver;
import usb.driver;
import usb.model;
import usb.plan;
import usb.runtime;
import usb.spec;

extern "C" {
#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "usb.h"

void SystemClock_Config(void);
}

namespace {
    using usb::u8;

    constexpr std::size_t kFsEndpointCount = 8;
    constexpr std::size_t kBulkOutBufferSize = 256;
    constexpr std::size_t kEp0BufferSize = 64;

    constexpr usb::u16 kLangs[] = {0x0409};
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Player USB Audio CDC");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("G431-0001");

    static const usb::StringTable<4> kStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    struct IrqGuard {
        IrqGuard() noexcept
            : primask_(__get_PRIMASK()) {
            __disable_irq();
        }

        ~IrqGuard() {
            __set_PRIMASK(primask_);
        }

        std::uint32_t primask_{0};
    };

    struct EndpointPipeState {
        bool opened{false};
        usb::driver::EpConfig cfg{};
        usb::driver::EpCallbacks callbacks{};
        std::size_t last_tx_len{0};
        bool zlp_pending{false};
        bool awaiting_zlp_complete{false};
    };

    struct EndpointState {
        EndpointPipeState in{};
        EndpointPipeState out{};
        std::array<u8, kBulkOutBufferSize> out_buffer{};
    };

    class Stm32FsPcdAdapter {
    public:
        explicit Stm32FsPcdAdapter(PCD_HandleTypeDef& hpcd) noexcept
            : hpcd_(hpcd) {
            hpcd_.pData = this;

            ops_.ep.open = &Stm32FsPcdAdapter::open_ep_cb;
            ops_.ep.close = &Stm32FsPcdAdapter::close_ep_cb;
            ops_.ep.send = &Stm32FsPcdAdapter::send_ep_cb;
            ops_.ep.stall = &Stm32FsPcdAdapter::stall_ep_cb;
            ops_.set_address = &Stm32FsPcdAdapter::set_address_cb;
            ops_.set_configured = &Stm32FsPcdAdapter::set_configured_cb;
            ops_.connect = &Stm32FsPcdAdapter::connect_cb;
        }

        [[nodiscard]] const usb::driver::DcdOps& dcd_ops() const noexcept { return ops_; }
        [[nodiscard]] usb::driver::DcdDeviceAdapter& device_adapter() noexcept { return adapter_; }
        [[nodiscard]] bool configured() const noexcept { return configured_; }
        [[nodiscard]] bool started() const noexcept { return started_; }

        static Stm32FsPcdAdapter* from(PCD_HandleTypeDef* hpcd) noexcept {
            return hpcd ? static_cast<Stm32FsPcdAdapter*>(hpcd->pData) : nullptr;
        }

        void on_setup_stage() noexcept {
            usb::SetupPacket setup{};
            std::memcpy(&setup, hpcd_.Setup, sizeof(setup));
            adapter_.handle_setup(setup);

            if (usb::request_direction(setup.bm_request_type) == usb::RequestDirection::out &&
                setup.w_length > 0 &&
                !hpcd_.OUT_ep[0].is_stall &&
                !hpcd_.IN_ep[0].is_stall) {
                const auto rx_len = (std::min<std::size_t>)(control_out_buffer_.size(), setup.w_length);
                (void)HAL_PCD_EP_Receive(
                    &hpcd_,
                    0x00,
                    control_out_buffer_.data(),
                    static_cast<std::uint32_t>(rx_len));
            }
        }

        void on_data_out_stage(u8 epnum) noexcept {
            if (epnum == 0) {
                const auto count = static_cast<std::size_t>(HAL_PCD_EP_GetRxCount(&hpcd_, 0x00));
                adapter_.handle_out_data(std::span<const u8>(control_out_buffer_.data(), count));
                return;
            }

            auto& state = endpoints_[epnum];
            if (!state.out.opened || !state.out.callbacks.on_out) {
                return;
            }

            const auto count = static_cast<std::size_t>(HAL_PCD_EP_GetRxCount(&hpcd_, epnum));
            state.out.callbacks.on_out(
                state.out.callbacks.ctx,
                std::span<const u8>(state.out_buffer.data(), count));

            (void)arm_out_endpoint(epnum);
        }

        void on_data_in_stage(u8 epnum) noexcept {
            if (epnum == 0) {
                if (ep0_awaiting_zlp_complete_) {
                    ep0_awaiting_zlp_complete_ = false;
                    adapter_.handle_in_complete(0, true);
                    return;
                }

                const auto sent = ep0_last_tx_len_;
                ep0_last_tx_len_ = 0;
                adapter_.handle_in_complete(sent, false);

                if (ep0_zlp_pending_) {
                    ep0_zlp_pending_ = false;
                    ep0_awaiting_zlp_complete_ = true;
                    (void)HAL_PCD_EP_Transmit(
                        &hpcd_,
                        0x80,
                        zero_buffer_.data(),
                        0);
                }
                return;
            }

            auto& pipe = endpoints_[epnum].in;
            if (!pipe.opened || !pipe.callbacks.on_in_complete) {
                return;
            }

            if (pipe.awaiting_zlp_complete) {
                pipe.awaiting_zlp_complete = false;
                pipe.callbacks.on_in_complete(pipe.callbacks.ctx, 0, true);
                return;
            }

            const auto sent = pipe.last_tx_len;
            pipe.last_tx_len = 0;
            pipe.callbacks.on_in_complete(pipe.callbacks.ctx, sent, false);

            if (pipe.zlp_pending) {
                pipe.zlp_pending = false;
                pipe.awaiting_zlp_complete = true;
                (void)HAL_PCD_EP_Transmit(
                    &hpcd_,
                    static_cast<u8>(0x80u | epnum),
                    zero_buffer_.data(),
                    0);
            }
        }

        void on_reset() noexcept {
            configured_ = false;
            ep0_last_tx_len_ = 0;
            ep0_zlp_pending_ = false;
            ep0_awaiting_zlp_complete_ = false;

            (void)HAL_PCD_SetAddress(&hpcd_, 0);
            (void)HAL_PCD_EP_Open(&hpcd_, 0x00, kEp0BufferSize, EP_TYPE_CTRL);
            (void)HAL_PCD_EP_Open(&hpcd_, 0x80, kEp0BufferSize, EP_TYPE_CTRL);

            for (auto& state : endpoints_) {
                state.in.last_tx_len = 0;
                state.in.zlp_pending = false;
                state.in.awaiting_zlp_complete = false;
            }

            adapter_.handle_reset();
        }

        void on_connect(bool connected) noexcept {
            adapter_.handle_connect(connected);
        }

        void on_suspend() noexcept {
            adapter_.handle_suspend();
        }

        void on_resume() noexcept {
            adapter_.handle_resume();
        }

    private:
        static EndpointPipeState& pipe_for(EndpointState& state, bool in_direction) noexcept {
            return in_direction ? state.in : state.out;
        }

        static std::uint8_t hal_ep_type(usb::driver::EpType type) noexcept {
            switch (type) {
            case usb::driver::EpType::control:
                return EP_TYPE_CTRL;
            case usb::driver::EpType::isochronous:
                return EP_TYPE_ISOC;
            case usb::driver::EpType::bulk:
                return EP_TYPE_BULK;
            case usb::driver::EpType::interrupt:
                return EP_TYPE_INTR;
            }
            return EP_TYPE_BULK;
        }

        bool arm_out_endpoint(u8 epnum) noexcept {
            auto& state = endpoints_[epnum];
            if (!state.out.opened) {
                return false;
            }

            const auto rx_len = (std::min<std::size_t>)(state.out_buffer.size(), state.out.cfg.max_packet_size);
            return HAL_PCD_EP_Receive(
                       &hpcd_,
                       epnum,
                       state.out_buffer.data(),
                       static_cast<std::uint32_t>(rx_len)) == HAL_OK;
        }

        bool open_endpoint(const usb::driver::EpConfig& cfg, usb::driver::EpCallbacks callbacks) noexcept {
            const auto epnum = static_cast<u8>(cfg.address & 0x0Fu);
            if (epnum >= endpoints_.size() || epnum == 0) {
                return false;
            }

            if (HAL_PCD_EP_Open(
                    &hpcd_,
                    cfg.address,
                    cfg.max_packet_size,
                    hal_ep_type(cfg.type)) != HAL_OK) {
                return false;
            }

            auto& state = endpoints_[epnum];
            auto& pipe = pipe_for(state, cfg.direction == usb::driver::EpDirection::in);
            pipe.opened = true;
            pipe.cfg = cfg;
            pipe.callbacks = callbacks;
            pipe.last_tx_len = 0;
            pipe.zlp_pending = false;
            pipe.awaiting_zlp_complete = false;

            if (cfg.direction == usb::driver::EpDirection::out) {
                return arm_out_endpoint(epnum);
            }

            return true;
        }

        bool close_endpoint(u8 address) noexcept {
            const auto epnum = static_cast<u8>(address & 0x0Fu);
            if (epnum >= endpoints_.size() || epnum == 0) {
                return false;
            }

            if (HAL_PCD_EP_Close(&hpcd_, address) != HAL_OK) {
                return false;
            }

            auto& state = endpoints_[epnum];
            auto& pipe = pipe_for(state, (address & 0x80u) != 0u);
            pipe = {};
            return true;
        }

        bool send_endpoint(u8 address, std::span<const u8> data, bool zlp) noexcept {
            if ((address & 0x7Fu) == 0u) {
                ep0_last_tx_len_ = data.size();
                ep0_zlp_pending_ = zlp && !data.empty();
                ep0_awaiting_zlp_complete_ = zlp && data.empty();
                return HAL_PCD_EP_Transmit(
                           &hpcd_,
                           0x80,
                           const_cast<u8*>(data.empty() ? zero_buffer_.data() : data.data()),
                           static_cast<std::uint32_t>(data.size())) == HAL_OK;
            }

            const auto epnum = static_cast<u8>(address & 0x0Fu);
            if (epnum >= endpoints_.size()) {
                return false;
            }

            auto& pipe = endpoints_[epnum].in;
            if (!pipe.opened) {
                return false;
            }

            pipe.last_tx_len = data.size();
            pipe.zlp_pending = zlp && !data.empty();
            pipe.awaiting_zlp_complete = zlp && data.empty();

            return HAL_PCD_EP_Transmit(
                       &hpcd_,
                       address,
                       const_cast<u8*>(data.empty() ? zero_buffer_.data() : data.data()),
                       static_cast<std::uint32_t>(data.size())) == HAL_OK;
        }

        bool stall_endpoint(u8 address) noexcept {
            return HAL_PCD_EP_SetStall(&hpcd_, address) == HAL_OK;
        }

        bool set_address(u8 address) noexcept {
            return HAL_PCD_SetAddress(&hpcd_, address) == HAL_OK;
        }

        bool set_configured(bool configured) noexcept {
            configured_ = configured;
            return true;
        }

        bool connect(bool enable) noexcept {
            started_ = enable;
            return enable ? (HAL_PCD_Start(&hpcd_) == HAL_OK)
                          : (HAL_PCD_Stop(&hpcd_) == HAL_OK);
        }

        static bool open_ep_cb(void* ctx, const usb::driver::EpConfig& cfg, usb::driver::EpCallbacks callbacks) noexcept {
            auto* self = static_cast<Stm32FsPcdAdapter*>(ctx);
            return self ? self->open_endpoint(cfg, callbacks) : false;
        }

        static bool close_ep_cb(void* ctx, u8 address) noexcept {
            auto* self = static_cast<Stm32FsPcdAdapter*>(ctx);
            return self ? self->close_endpoint(address) : false;
        }

        static bool send_ep_cb(void* ctx, u8 address, std::span<const u8> data, bool zlp) noexcept {
            auto* self = static_cast<Stm32FsPcdAdapter*>(ctx);
            return self ? self->send_endpoint(address, data, zlp) : false;
        }

        static bool stall_ep_cb(void* ctx, u8 address) noexcept {
            auto* self = static_cast<Stm32FsPcdAdapter*>(ctx);
            return self ? self->stall_endpoint(address) : false;
        }

        static bool set_address_cb(void* ctx, u8 address) noexcept {
            auto* self = static_cast<Stm32FsPcdAdapter*>(ctx);
            return self ? self->set_address(address) : false;
        }

        static bool set_configured_cb(void* ctx, bool configured) noexcept {
            auto* self = static_cast<Stm32FsPcdAdapter*>(ctx);
            return self ? self->set_configured(configured) : false;
        }

        static bool connect_cb(void* ctx, bool enable) noexcept {
            auto* self = static_cast<Stm32FsPcdAdapter*>(ctx);
            return self ? self->connect(enable) : false;
        }

        PCD_HandleTypeDef& hpcd_;
        usb::driver::DcdOps ops_{};
        usb::driver::DcdDeviceAdapter adapter_{};
        std::array<EndpointState, kFsEndpointCount> endpoints_{};
        std::array<u8, kEp0BufferSize> control_out_buffer_{};
        std::array<u8, 1> zero_buffer_{};
        std::size_t ep0_last_tx_len_{0};
        bool ep0_zlp_pending_{false};
        bool ep0_awaiting_zlp_complete_{false};
        bool configured_{false};
        bool started_{false};
    };

    struct CdcConsoleContext {
        std::array<u8, 512> tx{};
        std::array<u8, 256> rx{};
        std::size_t tx_len{0};
        std::size_t rx_len{0};
        usb::class_driver::CdcLineCoding last_line_coding{};
        usb::u16 control_line_state{0};
        bool ready{false};
        bool tx_in_flight{false};
        bool banner_sent{false};
        usb::class_driver::CdcAcm* cdc{nullptr};
        usb::class_driver::CdcConfig cdc_cfg{};

        void append_tx(std::span<const u8> data) noexcept {
            if (data.empty()) {
                return;
            }

            IrqGuard guard{};
            const auto avail = tx.size() - tx_len;
            const auto copy_len = (std::min<std::size_t>)(avail, data.size());
            if (copy_len == 0) {
                return;
            }

            std::memcpy(tx.data() + tx_len, data.data(), copy_len);
            tx_len += copy_len;
        }

        void append_text(const char* text) noexcept {
            if (!text) {
                return;
            }
            append_tx(std::span<const u8>(
                reinterpret_cast<const u8*>(text),
                std::strlen(text)));
        }

        void consume_rx_to_tx() noexcept {
            std::array<u8, 256> local{};
            std::size_t local_len = 0;

            {
                IrqGuard guard{};
                local_len = rx_len;
                if (local_len == 0) {
                    return;
                }
                std::memcpy(local.data(), rx.data(), local_len);
                rx_len = 0;
            }

            append_tx(std::span<const u8>(local.data(), local_len));
        }

        void poll(Stm32FsPcdAdapter& dcd) noexcept {
            if (!ready || cdc == nullptr || !dcd.configured()) {
                return;
            }

            if (!banner_sent && control_line_state != 0) {
                append_text("cdc ready\r\n");
                banner_sent = true;
            }

            consume_rx_to_tx();

            if (tx_in_flight || tx_len == 0) {
                return;
            }

            tx_in_flight = usb::device::examples::send_cdc_in_packet(
                dcd.dcd_ops(),
                &dcd,
                *cdc,
                cdc_cfg.ep_mps);
        }
    };

    std::span<u8> tx_buffer(void* ctx) noexcept {
        auto* self = static_cast<CdcConsoleContext*>(ctx);
        return self ? std::span<u8>(self->tx.data(), self->tx.size()) : std::span<u8>{};
    }

    std::span<u8> rx_buffer(void* ctx) noexcept {
        auto* self = static_cast<CdcConsoleContext*>(ctx);
        return self ? std::span<u8>(self->rx.data(), self->rx.size()) : std::span<u8>{};
    }

    std::size_t tx_length(void* ctx) noexcept {
        auto* self = static_cast<CdcConsoleContext*>(ctx);
        return self ? self->tx_len : 0;
    }

    void on_rx_done(void* ctx, std::size_t len) noexcept {
        auto* self = static_cast<CdcConsoleContext*>(ctx);
        if (!self) {
            return;
        }

        IrqGuard guard{};
        self->rx_len = (std::min<std::size_t>)(len, self->rx.size());
    }

    void on_tx_done(void* ctx, std::size_t len) noexcept {
        auto* self = static_cast<CdcConsoleContext*>(ctx);
        if (!self) {
            return;
        }

        IrqGuard guard{};
        self->tx_in_flight = false;
        if (len >= self->tx_len) {
            self->tx_len = 0;
            return;
        }

        std::memmove(self->tx.data(), self->tx.data() + len, self->tx_len - len);
        self->tx_len -= len;
    }

    void on_line_coding(void* ctx, const usb::class_driver::CdcLineCoding& coding) noexcept {
        auto* self = static_cast<CdcConsoleContext*>(ctx);
        if (self) {
            self->last_line_coding = coding;
        }
    }

    void on_control_line(void* ctx, usb::u16 value) noexcept {
        auto* self = static_cast<CdcConsoleContext*>(ctx);
        if (self) {
            self->control_line_state = value;
        }
    }

    void on_ready(void* ctx,
                  usb::class_driver::CdcAcm* cdc,
                  const usb::class_driver::CdcConfig* cfg) noexcept {
        auto* self = static_cast<CdcConsoleContext*>(ctx);
        if (!self || cdc == nullptr || cfg == nullptr) {
            return;
        }

        self->cdc = cdc;
        self->cdc_cfg = *cfg;
        self->ready = true;
    }

    usb::class_driver::CdcOps make_cdc_ops() noexcept {
        usb::class_driver::CdcOps ops{};
        ops.tx_buffer = &tx_buffer;
        ops.rx_buffer = &rx_buffer;
        ops.tx_length = &tx_length;
        ops.on_rx_done = &on_rx_done;
        ops.on_tx_done = &on_tx_done;
        ops.on_line_coding = &on_line_coding;
        ops.on_control_line = &on_control_line;
        return ops;
    }

    [[noreturn]] void fail_fast() {
        Error_Handler();
        while (true) {
        }
    }
}

extern "C" void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    if (auto* adapter = Stm32FsPcdAdapter::from(hpcd)) {
        adapter->on_setup_stage();
    }
}

extern "C" void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, std::uint8_t epnum) {
    if (auto* adapter = Stm32FsPcdAdapter::from(hpcd)) {
        adapter->on_data_out_stage(epnum);
    }
}

extern "C" void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, std::uint8_t epnum) {
    if (auto* adapter = Stm32FsPcdAdapter::from(hpcd)) {
        adapter->on_data_in_stage(epnum);
    }
}

extern "C" void HAL_PCD_ResetCallback(PCD_HandleTypeDef* hpcd) {
    if (auto* adapter = Stm32FsPcdAdapter::from(hpcd)) {
        adapter->on_reset();
    }
}

extern "C" void HAL_PCD_ConnectCallback(PCD_HandleTypeDef* hpcd) {
    if (auto* adapter = Stm32FsPcdAdapter::from(hpcd)) {
        adapter->on_connect(true);
    }
}

extern "C" void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef* hpcd) {
    if (auto* adapter = Stm32FsPcdAdapter::from(hpcd)) {
        adapter->on_connect(false);
    }
}

extern "C" void HAL_PCD_SuspendCallback(PCD_HandleTypeDef* hpcd) {
    if (auto* adapter = Stm32FsPcdAdapter::from(hpcd)) {
        adapter->on_suspend();
    }
}

extern "C" void HAL_PCD_ResumeCallback(PCD_HandleTypeDef* hpcd) {
    if (auto* adapter = Stm32FsPcdAdapter::from(hpcd)) {
        adapter->on_resume();
    }
}

int main() {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USB_PCD_Init();

    static Stm32FsPcdAdapter usb_dcd{hpcd_USB_FS};
    static CdcConsoleContext cdc_console{};

    usb::spec::DeviceSpec device_spec{};
    device_spec.vendor_id = 0xCAFE;
    device_spec.product_id = 0x4310;
    device_spec.i_manufacturer = 1;
    device_spec.i_product = 2;
    device_spec.i_serial = 3;
    device_spec.strings = std::span<const std::span<const usb::u8>>(
        kStrings.entries.data(),
        kStrings.entries.size());

    usb::spec::CdcFunctionSpec cdc_spec{};
    cdc_spec.cap_name = "usb.cdc0";
    cdc_spec.ep_notify = 0x81;
    cdc_spec.ep_out = 0x01;
    cdc_spec.ep_in = 0x82;
    cdc_spec.ep_mps = 64;

    const auto spec = usb::spec::cdc_device(device_spec, cdc_spec);
    const auto model = usb::build(spec);
    const auto plan = usb::plan::build(model);
    if (!plan) {
        fail_fast();
    }

    const auto runtime = usb::runtime::stm32_fs(
        usb_dcd.dcd_ops(),
        &usb_dcd,
        &usb_dcd.device_adapter(),
        {},
        usb::runtime::CdcRuntimeConfig{
            &cdc_console,
            make_cdc_ops(),
            usb::runtime::CdcReadyHook{&on_ready, &cdc_console},
        });

    auto binding = usb::runtime::make(plan.value(), runtime);
    const auto init = decltype(binding)::init_trampoline(&binding);
    if (!init) {
        fail_fast();
    }

    while (true) {
        cdc_console.poll(usb_dcd);
        __WFI();
    }
}
