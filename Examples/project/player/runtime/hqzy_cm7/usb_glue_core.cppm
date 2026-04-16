module;

#define CHARM_ALLOW_HAL 1

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "stm32h7xx_hal.h"

export module player.stm32h7.usb_glue_core;

import usb.common;
import usb.device_driver;
import usb.driver;

export namespace player::stm32h7::usb_glue_core {
    enum class ConnectMode : std::uint8_t {
        device_connect,
        start_stop,
    };

    struct Core {
        usb::driver::DcdDeviceAdapter adapter{};
        usb::driver::DcdOps dcd_ops{};
        std::array<usb::driver::EpCallbacks, 16> out_cbs{};
        std::array<usb::driver::EpCallbacks, 16> in_cbs{};
        std::array<void*, 16> out_ctxs{};
        std::array<void*, 16> in_ctxs{};
        std::array<std::array<usb::u8, 64>, 16> out_bufs{};
        std::array<usb::u16, 16> out_mps{};
        std::array<bool, 16> in_busy{};
        PCD_HandleTypeDef* pcd{nullptr};
        bool ep0_prepared{false};
        bool ep0_expect_status_out{false};
        bool ep0_in_zlp_pending{false};
    };

    struct SetupView {
        std::array<std::uint8_t, 8> raw{};
        usb::SetupPacket packet{};
    };

    struct DataOutEvent {
        std::uint8_t epnum{0};
        std::uint32_t len{0};
        const usb::u8* buf{nullptr};
    };

    struct DataInEvent {
        std::uint8_t epnum{0};
        std::uint32_t sent{0};
        bool sent_zlp{false};
    };

    inline Core* from_pcd(PCD_HandleTypeDef* pcd) noexcept {
        return pcd ? static_cast<Core*>(pcd->pData) : nullptr;
    }

    inline void bind_pcd(Core& core, PCD_HandleTypeDef* pcd) noexcept {
        core.pcd = pcd;
        if (core.pcd) {
            core.pcd->pData = &core;
        }
    }

    inline void clear_in_busy(Core& core) noexcept {
        for (auto& busy : core.in_busy) {
            busy = false;
        }
    }

    inline void prepare_ep0(Core& core) noexcept {
        if (!core.pcd || core.ep0_prepared) return;
        constexpr std::uint8_t ep0_mps = 64;
        core.out_mps[0] = ep0_mps;
        core.in_busy[0] = false;
        (void)HAL_PCD_EP_Open(core.pcd, 0x00, ep0_mps, EP_TYPE_CTRL);
        (void)HAL_PCD_EP_Open(core.pcd, 0x80, ep0_mps, EP_TYPE_CTRL);
        core.pcd->IN_ep[0].data_pid_start = 1;
        core.pcd->OUT_ep[0].data_pid_start = 1;
        (void)HAL_PCD_EP_Receive(core.pcd, 0x00, core.out_bufs[0].data(), core.out_mps[0]);
        core.ep0_expect_status_out = false;
        core.ep0_in_zlp_pending = false;
        core.ep0_prepared = true;
    }

    inline bool ep_open(Core& core,
                        PCD_HandleTypeDef* pcd,
                        const usb::driver::EpConfig& cfg,
                        usb::driver::EpCallbacks cb) noexcept {
        if (!pcd) return false;
        std::uint8_t type = EP_TYPE_BULK;
        switch (cfg.type) {
        case usb::driver::EpType::control: type = EP_TYPE_CTRL; break;
        case usb::driver::EpType::isochronous: type = EP_TYPE_ISOC; break;
        case usb::driver::EpType::bulk: type = EP_TYPE_BULK; break;
        case usb::driver::EpType::interrupt: type = EP_TYPE_INTR; break;
        }
        if (HAL_PCD_EP_Open(pcd, cfg.address, cfg.max_packet_size, type) != HAL_OK) {
            return false;
        }
        const std::uint8_t ep_num = static_cast<std::uint8_t>(cfg.address & 0x0F);
        if (cfg.direction == usb::driver::EpDirection::out) {
            core.out_cbs[ep_num] = cb;
            core.out_ctxs[ep_num] = cb.ctx;
            core.out_mps[ep_num] = cfg.max_packet_size;
            (void)HAL_PCD_EP_Receive(pcd, cfg.address,
                core.out_bufs[ep_num].data(),
                core.out_mps[ep_num]);
        } else {
            core.in_cbs[ep_num] = cb;
            core.in_ctxs[ep_num] = cb.ctx;
            core.in_busy[ep_num] = false;
        }
        return true;
    }

    inline bool ep_close(Core& core, PCD_HandleTypeDef* pcd, usb::u8 address) noexcept {
        if (!pcd) return false;
        if (HAL_PCD_EP_Close(pcd, address) != HAL_OK) return false;
        const std::uint8_t ep_num = static_cast<std::uint8_t>(address & 0x0F);
        if ((address & 0x80u) != 0u) {
            core.in_cbs[ep_num] = {};
            core.in_ctxs[ep_num] = nullptr;
            core.in_busy[ep_num] = false;
        } else {
            core.out_cbs[ep_num] = {};
            core.out_ctxs[ep_num] = nullptr;
        }
        return true;
    }

    inline bool ep_send(Core& core,
                        PCD_HandleTypeDef* pcd,
                        usb::u8 address,
                        std::span<const usb::u8> data,
                        bool zlp) noexcept {
        if (!pcd) return false;
        const auto ep_num = static_cast<std::uint8_t>(address & 0x0F);
        auto* ptr = const_cast<usb::u8*>(data.data());
        const auto ok = HAL_PCD_EP_Transmit(pcd, address, ptr,
            static_cast<uint16_t>(data.size())) == HAL_OK;
        if (address == 0x80u) {
            core.ep0_in_zlp_pending = ok && zlp && !data.empty();
        }
        if (ok && (address & 0x80u) != 0u) {
            core.in_busy[ep_num] = true;
        }
        return ok;
    }

    inline bool ep_stall(PCD_HandleTypeDef* pcd, usb::u8 address) noexcept {
        return pcd && (HAL_PCD_EP_SetStall(pcd, address) == HAL_OK);
    }

    inline bool set_address(PCD_HandleTypeDef* pcd, usb::u8 address) noexcept {
        return pcd && (HAL_PCD_SetAddress(pcd, address) == HAL_OK);
    }

    inline bool set_configured(Core& core,
                               PCD_HandleTypeDef* pcd,
                               bool configured) noexcept {
        if (!pcd) return false;
        (void)configured;
        clear_in_busy(core);
        return true;
    }

    inline bool rearm_out(Core& core,
                          PCD_HandleTypeDef* pcd,
                          usb::u8 address) noexcept {
        if (!pcd || (address & 0x80u) != 0u) return false;
        const auto ep_num = static_cast<std::uint8_t>(address & 0x0F);
        const auto mps = core.out_mps[ep_num];
        if (mps == 0u) return false;
        return HAL_PCD_EP_Receive(pcd, address,
            core.out_bufs[ep_num].data(),
            mps) == HAL_OK;
    }

    inline bool rearm_out(Core& core, usb::u8 address) noexcept {
        return rearm_out(core, core.pcd, address);
    }

    inline bool endpoint_busy(const Core& core, usb::u8 address) noexcept {
        if ((address & 0x80u) == 0u) return false;
        return core.in_busy[static_cast<std::uint8_t>(address & 0x0F)];
    }

    inline bool connect(Core& core,
                        PCD_HandleTypeDef* pcd,
                        bool enable,
                        ConnectMode mode) noexcept {
        if (!pcd) return false;
        if (enable) {
            prepare_ep0(core);
        }
        if (mode == ConnectMode::device_connect) {
            return enable ? (HAL_PCD_DevConnect(pcd) == HAL_OK)
                          : (HAL_PCD_DevDisconnect(pcd) == HAL_OK);
        }
        return enable ? (HAL_PCD_Start(pcd) == HAL_OK)
                      : (HAL_PCD_Stop(pcd) == HAL_OK);
    }

    inline SetupView decode_setup(PCD_HandleTypeDef* hpcd) noexcept {
        SetupView view{};
        if (!hpcd) return view;
        for (std::size_t i = 0; i < view.raw.size(); ++i) {
            view.raw[i] = hpcd->Setup[i];
        }
        view.packet.bm_request_type = view.raw[0];
        view.packet.b_request = view.raw[1];
        view.packet.w_value = static_cast<usb::u16>(view.raw[2] | (view.raw[3] << 8));
        view.packet.w_index = static_cast<usb::u16>(view.raw[4] | (view.raw[5] << 8));
        view.packet.w_length = static_cast<usb::u16>(view.raw[6] | (view.raw[7] << 8));
        return view;
    }

    inline int handle_setup(Core& core,
                            PCD_HandleTypeDef* hpcd,
                            const SetupView& view) noexcept {
        if (!hpcd) return 0;
        if (usb::request_type(view.packet.bm_request_type) == usb::RequestType::standard &&
                view.packet.b_request == 0x01 &&
                (view.packet.bm_request_type & 0x1Fu) == 0x02) {
            const auto ep = static_cast<std::uint8_t>(view.packet.w_index & 0xFFu);
            (void)HAL_PCD_EP_ClrStall(hpcd, ep);
        }
        core.ep0_expect_status_out = ((view.packet.bm_request_type & 0x80u) != 0u);
        core.adapter.handle_setup(view.packet);
        if ((view.packet.bm_request_type & 0x80u) == 0u) {
            (void)HAL_PCD_EP_Receive(hpcd, 0x00, core.out_bufs[0].data(), core.out_mps[0]);
        }
        return 1;
    }

    inline DataOutEvent inspect_data_out(PCD_HandleTypeDef* hpcd, std::uint8_t epnum) noexcept {
        DataOutEvent event{};
        event.epnum = epnum;
        if (!hpcd) return event;
        if (epnum == 0u) {
            event.len = hpcd->OUT_ep[0].xfer_count;
            event.buf = hpcd->OUT_ep[0].xfer_buff;
            return event;
        }
        event.len = hpcd->OUT_ep[epnum].xfer_count;
        return event;
    }

    inline int handle_data_out(Core& core,
                               PCD_HandleTypeDef* hpcd,
                               const DataOutEvent& event) noexcept {
        if (!hpcd) return 0;
        if (event.epnum == 0u) {
            if (event.buf && event.len > 0u) {
                core.adapter.handle_out_data(std::span<const usb::u8>(event.buf, event.len));
            } else {
                core.adapter.handle_out_data(std::span<const usb::u8>{});
            }
            if (event.len == 0u && core.ep0_expect_status_out) {
                core.ep0_expect_status_out = false;
            }
            return 1;
        }
        auto& cb = core.out_cbs[event.epnum];
        if (cb.on_out && event.len > 0u) {
            cb.on_out(core.out_ctxs[event.epnum],
                std::span<const usb::u8>(core.out_bufs[event.epnum].data(), event.len));
        }
        const auto addr = static_cast<std::uint8_t>(event.epnum & 0x0F);
        (void)HAL_PCD_EP_Receive(hpcd, addr,
            core.out_bufs[event.epnum].data(),
            core.out_mps[event.epnum]);
        return 1;
    }

    inline DataInEvent inspect_data_in(PCD_HandleTypeDef* hpcd, std::uint8_t epnum) noexcept {
        DataInEvent event{};
        event.epnum = epnum;
        if (!hpcd) return event;
        if (epnum == 0u) {
            event.sent_zlp = (hpcd->IN_ep[0].xfer_len == 0u);
            event.sent = event.sent_zlp
                ? 0u
                : static_cast<std::uint32_t>(hpcd->IN_ep[0].xfer_len);
            return event;
        }
        event.sent = static_cast<std::uint32_t>(hpcd->IN_ep[epnum].xfer_len);
        return event;
    }

    inline int handle_data_in(Core& core,
                              PCD_HandleTypeDef* hpcd,
                              const DataInEvent& event) noexcept {
        if (!hpcd) return 0;
        if (event.epnum == 0u) {
            if (!event.sent_zlp && core.ep0_in_zlp_pending) {
                core.adapter.handle_in_complete(event.sent, false);
                core.ep0_in_zlp_pending = false;
                (void)HAL_PCD_EP_Transmit(hpcd, 0x80, nullptr, 0);
                return 1;
            }
            core.in_busy[0] = false;
            if (core.ep0_expect_status_out) {
                (void)HAL_PCD_EP_Receive(hpcd, 0x00, core.out_bufs[0].data(), 0);
            } else {
                (void)HAL_PCD_EP_Receive(hpcd, 0x00, core.out_bufs[0].data(), core.out_mps[0]);
            }
            core.adapter.handle_in_complete(event.sent, event.sent_zlp);
            return 1;
        }
        core.in_busy[event.epnum] = false;
        auto& cb = core.in_cbs[event.epnum];
        if (cb.on_in_complete) {
            cb.on_in_complete(core.in_ctxs[event.epnum], event.sent, false);
        }
        return 1;
    }

    inline int handle_reset(Core& core, PCD_HandleTypeDef* hpcd) noexcept {
        if (!hpcd) return 0;
        bind_pcd(core, hpcd);
        clear_in_busy(core);
        core.ep0_prepared = false;
        core.ep0_expect_status_out = false;
        core.ep0_in_zlp_pending = false;
        prepare_ep0(core);
        core.adapter.handle_reset();
        return 1;
    }

    inline int handle_suspend(Core& core) noexcept {
        core.adapter.handle_suspend();
        return 1;
    }

    inline int handle_resume(Core& core) noexcept {
        core.adapter.handle_resume();
        return 1;
    }

    inline int handle_connect(Core& core, bool connected) noexcept {
        core.adapter.handle_connect(connected);
        return 1;
    }
}
