#include <cstdint>
#include <cstring>
#include <string_view>
#include <optional>

#include "main.h"
#include "tim.h"
#include "i2c.h"
/*
LED
    StateLED-> PI15

UART
    PA10     ------> USART1_RX
    PA9     ------> USART1_TX

OLED
    PB10     ------> I2C2_SCL
    PB11     ------> I2C2_SDA

Encoder
    PI7     ------> Key
    PI6     ------> TIM8_CH2
    PI5     ------> TIM8_CH1

Debug
    PA14 (JTCK/SWCLK)   ------> DEBUG_JTCK-SWCLK
    PA13 (JTMS/SWDIO)   ------> DEBUG_JTMS-SWDIO
 */

import out.api;

// out::port::console_sink console;

static std::uint32_t tick_now(void*) noexcept
{
    return HAL_GetTick();
}

static volatile bool s_i2c_dma_busy = false;
static std::uint32_t s_i2c_dma_start_ms = 0;
static std::uint8_t s_i2c_dma_frame[1 + 128 * 64 / 8]{};
static bool s_i2c_dma_enabled = true;


class SSD1306 {
public:
    enum { CMD, DATA };
    enum { I2C_CMD_ADDR = 0x00, I2C_DATA_ADDR = 0x40 };
    static constexpr uint8_t I2C_ADDR_default = 0x3C << 1;
    static constexpr uint8_t I2C_ADDR = 0x3D << 1;
    // 显示尺寸常量
    static constexpr uint8_t Width = 128;
    static constexpr uint8_t Height = 64;
    static constexpr uint8_t Pages = Height / 8;

    uint8_t buffer[Width * Height / 8]{}; // 128 x 64 / 8 = 1024 Byte

    explicit SSD1306 (void (transmit)(uint8_t *data, uint16_t len, bool cmd_or_data)) : transmit(transmit) {}

    void write_bytes(uint8_t dat, const bool cmd) const { transmit(&dat, 1, cmd); }

    void write_bytes(uint8_t *dat, const bool cmd, const uint16_t len) const { transmit(dat, len, cmd); }

    void init() const
    {
        // 初始化指令序列
        constexpr uint8_t init_cmds[] = {
            0xAE,       // 关闭显示
            0xD5, 0x80, // 设置显示时钟分频
            0xA8, 0x3F, // 设置多路复用率  1/64倍
            0xD3, 0x00, // 设置显示偏移
            0x40,       // 设置显示起始行
            0x8D, 0x14, // 电荷泵设置
            0x20, 0x00, // 内存地址模式   水平地址模式0x00  页地址0x02
            0xA1,       // 段重映射
            0xC8,       // 扫描方向
            0xDA, 0x12, // COM引脚硬件配置
            0x81, 0xCF, // 对比度设置
            0xD9, 0xF1, // 预充电周期
            0xDB, 0x40, // VCOMH设置
            0xA4,       // 显示内容跟随RAM
            0xA6,       // 正常显示
            0xAF        // 打开显示
        };
        write_bytes(const_cast<uint8_t*>(init_cmds), CMD, sizeof(init_cmds));
    }

    void refresh_page() const
    {
#if 0
        for (uint8_t page = 0; page < 8; page++) {
            write_bytes(0xB0 + page, CMD);// 设置页地址（0xB0 ~ 0xB7）
            write_bytes(0x00, CMD);// 设置低列地址
            write_bytes(0x10, CMD);// 设置高列地址

            for (uint8_t col = 0; col < 128; col++) { write_bytes(buffer[page * 128 + col], DATA); }
        }
#elif 0
        // 预计算页起始地址
        static const uint8_t page_addrs[] = {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7};

        for (uint8_t page = 0; page < 8; page++) {
            uint8_t cmd_seq[3] = {
                page_addrs[page], // 页地址
                0x00,             // 低列地址
                0x10              // 高列地址
            };
            write_bytes(cmd_seq, CMD, 3);  // 合并发送3个命令

            write_bytes(const_cast<uint8_t*>(&buffer[page * 128]), DATA, 128);
            // write_bytes(const_cast<uint8_t*>(buffer + page * 128), DATA, 128);
        }
#elif 1 // 优化版 全屏一次性刷新，但可能有兼容问题
        // 设置连续写入模式
        write_bytes(const_cast<uint8_t*>(init_seq), CMD, sizeof(init_seq));

        write_bytes(const_cast<uint8_t*>(buffer), DATA, sizeof(buffer));
#endif
    }

    bool refresh_page_dma() const
    {
        if (s_i2c_dma_busy) return false;
        write_bytes(const_cast<uint8_t*>(init_seq), CMD, sizeof(init_seq));
        s_i2c_dma_busy = true;
        s_i2c_dma_frame[0] = I2C_DATA_ADDR;
        std::memcpy(&s_i2c_dma_frame[1], buffer, sizeof(buffer));
        if (HAL_I2C_Master_Transmit_DMA(&hi2c2,
                                        SSD1306::I2C_ADDR_default,
                                        s_i2c_dma_frame,
                                        sizeof(s_i2c_dma_frame)) != HAL_OK) {
            s_i2c_dma_busy = false;
            return false;
        }
        return true;
    }

    void clear(const uint8_t fill, const bool refresh = true)
    {
        for (auto &i: buffer) { i = fill; }
        if (refresh) refresh_page();
    }

    void invert_display(const bool enable) const { write_bytes(enable ? 0xA7 : 0xA6,CMD); }

    void draw_pixel(const int x, const int y, const bool on = true) {
        if (x < 0 || x >= Width || y < 0 || y >= Height) return;

        // int page = y / 8;
        // int bit = y % 8;
        // buffer[page * 128 + x] |= (1 << bit);

        const uint16_t idx = x + (y / 8) * Width;
        const uint8_t mask = 1 << (y % 8);
        buffer[idx] = on ? (buffer[idx] | mask) : (buffer[idx] & ~mask);
    }

    void draw_rect(const uint8_t x1, const uint8_t y1, const uint8_t x2, const uint8_t y2, const bool fill = false) {
        for (uint8_t x = x1; x <= x2; ++x) {
            for (uint8_t y = y1; y <= y2; ++y) {
                if (fill || x == x1 || x == x2 || y == y1 || y == y2) {
                    draw_pixel(x, y);
                }
            }
        }
    }

private:
    void (*transmit)(uint8_t *data, uint16_t len, bool data_or_cmd);

    static constexpr uint8_t init_seq[] = {
        0x20, 0x00,        // 水平寻址模式
        0x21, 0x00, 0x7F,  // 设置列地址范围（0-127）
        0x22, 0x00, 0x07   // 设置页地址范围（0-7）
    };
};

extern "C" void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    if (hi2c == &hi2c2) {
        s_i2c_dma_busy = false;
    }
}

extern "C" void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
    if (hi2c == &hi2c2) {
        s_i2c_dma_busy = false;
    }
}

extern "C" void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* hi2c)
{
    if (hi2c == &hi2c2) {
        s_i2c_dma_busy = false;
    }
}

static SSD1306 display([](uint8_t *data, const uint16_t len, const bool data_or_cmd) {
        HAL_I2C_Mem_Write(&hi2c2, SSD1306::I2C_ADDR_default, data_or_cmd == SSD1306::CMD ? SSD1306::I2C_CMD_ADDR : SSD1306::I2C_DATA_ADDR, 1, data, len, HAL_MAX_DELAY);
});


static int32_t last_cnt = 0;
static int32_t acc = 0;
static constexpr int32_t STEP = 4; // 实测一格=4

inline int32_t Encoder_GetPos() { return static_cast<int32_t>(__HAL_TIM_GET_COUNTER(&htim8)); }

int32_t Encoder_GetDelta()
{
    const auto    cnt   = static_cast<int32_t>(__HAL_TIM_GET_COUNTER(&htim8));
    const int32_t delta = static_cast<int16_t>(cnt - last_cnt); // 若 TIM 是 16-bit， 16-bit 回绕安全
    last_cnt            = cnt;
    return delta;
}

// 返回“菜单步进”：-N..+N（通常是-1/0/+1，转很快可能是2、3）
int32_t Encoder_GetStep()
{
    int32_t d = Encoder_GetDelta();

    // 10ms里跳特别大一般是干扰/抖动（可按手感调阈值）
    if (d > 20 || d < -20) return 0;

    acc += d;

    int32_t step = 0;
    while (acc >= STEP) { acc -= STEP; step += 1; }
    while (acc <= -STEP){ acc += STEP; step -= 1; }

    return step;
}





import gui.canvas_1bpp;
import gui.renderer;
import gui.perf;
import gui.ui_settings;
import gui.ui_input_policy;
import app.state;
import app.ui;
import app.logic_intent;

import gui.input;
import input.router;

namespace gui_input = gui::input;


struct RawSourceSTM32 {
    const bool* ks{nullptr};
    int nkeys{0};

    int logical_w{0};
    int logical_h{0};
    int win_w{0};
    int win_h{0};

    // AB 相位队列（固定容量，零动态）---
    static constexpr int QN = 256;  // 足够大：每 detent 5 相位，256 可缓存 ~50 detents
    std::uint8_t q_[QN]{};
    int qh_{0}, qt_{0}, qc_{0};

    // 调试计数器
    int total_phases_pushed_{0};

    bool key_down_[4]{false,false,false,false};
    bool use_event_keys_{false};

    RawSourceSTM32(int lw = 0, int lh = 0, int initial_scale = 1) noexcept
        : logical_w(lw),
          logical_h(lh),
          win_w(lw * ((initial_scale > 0) ? initial_scale : 1)),
          win_h(lh * ((initial_scale > 0) ? initial_scale : 1)) {}

    // push 一个 AB 状态（0..3）到队列
    bool push_ab(std::uint8_t ab) noexcept {
        if (qc_ >= QN) {
            return false;
        }
        q_[qt_] = (ab & 0x03);
        qt_ = (qt_ + 1) % QN;
        ++qc_;
        total_phases_pushed_++;

        return true;
    }

    // pop 一个 AB 状态；空则返回 nullopt
    std::optional<std::uint8_t> pop_encoder_ab() noexcept {
        if (qc_ == 0) return std::nullopt;
        const std::uint8_t v = q_[qh_];
        qh_ = (qh_ + 1) % QN;
        --qc_;

        return v;
    }


    gui_input::PointerRaw read_pointer() const noexcept { return gui_input::PointerRaw{}; }
    gui_input::AxisRaw    read_axis() const noexcept { return gui_input::AxisRaw{0, 0}; }

    // --- RawSource 契约：每帧 update() 一次 ---
    void update(std::uint32_t now_ms) noexcept {
        (void)now_ms;
        // ---- Enter 键 ----
        bool down = (HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_7) == GPIO_PIN_RESET);
        key_down_[static_cast<size_t>(gui_input::Button::Enter)] = down;

        int32_t step = Encoder_GetStep();   // 这里是“菜单步进”，已经把 4 折算掉了
        if (step == 0) return;

        static constexpr std::uint8_t seq_cw[5]  = {0, 1, 3, 2, 0};
        static constexpr std::uint8_t seq_ccw[5] = {0, 2, 3, 1, 0};

        auto emit = [&](bool cw) {
            const auto* seq = cw ? seq_cw : seq_ccw;
            for (int i = 0; i < 5; ++i) {
                if (!push_ab(seq[i])) return; // 先不 fatal，避免偶发溢出直接死机
            }
        };

        if (step > 0) {
            while (step--) emit(true);
        } else {
            while (step++) emit(false);
        }
    }

    // --- Buttons ---
    bool is_down(gui_input::Button b) const noexcept {
        const int idx = (b == gui_input::Button::Up) ? 0 : (b == gui_input::Button::Down) ? 1 : (b == gui_input::Button::Enter) ? 2 : 3;
        return key_down_[idx];
        return false;
    }
};


static inline bool canvas_get_pixel(const gui::Canvas1bpp<128,64>& c, int x, int y) {
    // 按你 Canvas1bpp 的规则读：row-major，1字节=同一行的8个水平像素，MSB在左
    auto bytes = c.bytes().data();
    constexpr int stride = 128 / 8;                 // 16
    const int bi = y * stride + (x >> 3);
    const uint8_t m = uint8_t(0x80u >> (x & 7));
    return (bytes[bi] & m) != 0;
}

static void canvas_to_ssd1306_buffer(const gui::Canvas1bpp<128,64>& c, uint8_t* out1024) {
    for (int page = 0; page < 8; ++page) {
        for (int x = 0; x < 128; ++x) {
            uint8_t v = 0;
            for (int bit = 0; bit < 8; ++bit) {
                int y = page * 8 + bit;
                if (canvas_get_pixel(c, x, y)) {
                    v |= uint8_t(1u << bit);       // 若上下颠倒，把 bit 改成 (7-bit)
                }
            }
            out1024[page * 128 + x] = v;
        }
    }
}

static void canvas_to_ssd1306_partial(const gui::Canvas1bpp<128,64>& c,
                                      uint8_t* out1024,
                                      int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 128) x1 = 128;
    if (y1 > 64) y1 = 64;
    if (x1 <= x0 || y1 <= y0) return;

    const int page0 = y0 / 8;
    const int page1 = (y1 - 1) / 8;

    for (int page = page0; page <= page1; ++page) {
        const int y_base = page * 8;
        for (int x = x0; x < x1; ++x) {
            uint8_t v = 0;
            for (int bit = 0; bit < 8; ++bit) {
                const int y = y_base + bit;
                if (y < 0 || y >= 64) continue;
                if (canvas_get_pixel(c, x, y)) {
                    v |= uint8_t(1u << bit);
                }
            }
            out1024[page * 128 + x] = v;
        }
    }
}

static void refresh_area(SSD1306& display, int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 128) x1 = 128;
    if (y1 > 64) y1 = 64;
    if (x1 <= x0 || y1 <= y0) return;

    const int page0 = y0 / 8;
    const int page1 = (y1 - 1) / 8;
    const int width = x1 - x0;

    for (int page = page0; page <= page1; ++page) {
        const uint8_t cmd_seq[3] = {
            (uint8_t)(0xB0 + page),
            (uint8_t)(0x00 + (x0 & 0x0F)),
            (uint8_t)(0x10 + ((x0 >> 4) & 0x0F))
        };
        display.write_bytes(const_cast<uint8_t*>(cmd_seq), SSD1306::CMD, 3);
        display.write_bytes(&display.buffer[page * 128 + x0], SSD1306::DATA, (uint16_t)width);
    }
}


extern "C" void application()
{
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOI, &gpio);

    HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
    display.init();
    display.clear(0);//已确认显示正常。

    using Canvas = gui::Canvas1bpp<128, 64>;
    Canvas canvas;
    gui::Renderer<Canvas> renderer(canvas);

    app::AppState state{};
    state.init();

    app::set_tick_source(state, gui::perf::make_tick_source(tick_now, nullptr));
    state.data.battery = 100;
    state.ui.fps_overlay = gui::ui::Toggle::On;

    RawSourceSTM32 raw(128, 64);
    ::gui_input::Router router{};
    gui::input::RawSampler raw_sampler{};
    gui::ui::RouterIntentQueue<> router_queue{};
    (void)router_queue.start(router);
    const auto router_policy = router_queue.policy();
    state.input_policies.set(gui::ui::InputPolicyId::Default, router_policy);
    state.input_policies.set(gui::ui::InputPolicyId::Encoder, router_policy);
    static gui::ui::PolicyChain<2> policy_chain{};
    policy_chain.clear();
    policy_chain.add(router_policy);
    state.input_policies.set(gui::ui::InputPolicyId::Custom, gui::ui::make_policy_chain(policy_chain));
    state.input_policy_id = gui::ui::InputPolicyId::Default;
    state.input_policy = state.input_policies.get(state.input_policy_id);
    int last_dirty_count = 0;
    int last_dirty_area = 0;
    bool last_dirty_full = false;

    while (true) {
        auto ms = HAL_GetTick();
        state.now_ms = ms;
        state.fps_ui.update(state.tick);
        raw.update(ms);
        while (auto ev = raw_sampler.poll(raw, ms)) {
            router.dispatch(*ev);
        }

        // 3) 消费意图
        app::pump_input(state, ms);

        // 电量缓慢往复（demo）：仅在 Main 页演示，避免覆盖 Battery 详情页的手动调节
        if (state.pages.current() == app::PageId::Main){
            const std::uint32_t period = 6000;
            const std::uint32_t m = ms % period;
            int b = (m < period / 2) ? (100 - (int)(m * 100 / (period/2)))
                                    : (int)((m - period/2) * 100 / (period/2));
            if (b < 0) {
                b = 0;
            }
            if (b > 100) {
                b = 100;
            }
            state.data.battery = b;
            state.data.progress_demo = (std::uint8_t)b;
        }

        // --- 画 UI ---
        app::draw_current_ui(renderer, state);
        if (state.ui.fps_overlay == gui::ui::Toggle::On) {
            char dbg_buf[24]{};
            
            std::snprintf(dbg_buf, sizeof(dbg_buf), "D:%d A:%d%s",
                          last_dirty_count, last_dirty_area, last_dirty_full ? "F" : "");
            renderer.drawText(2, 2, dbg_buf, true);
        }

        // --- 显示 ---
        if (s_i2c_dma_busy) {
            if ((ms - s_i2c_dma_start_ms) > 30u) {
                s_i2c_dma_busy = false;
            }
        }

        if (canvas.dirty_count() > 0) {
            constexpr int kDirtyMaxRects = 4;
            constexpr int kDirtyAreaLimit = (128 * 64) / 2;
            const auto stats = canvas.dirty_stats();
            const bool too_many = (stats.count > kDirtyMaxRects);
            const bool too_big = (stats.area > kDirtyAreaLimit);
            const bool full = stats.full || too_many || too_big;
            bool refreshed = false;
            last_dirty_count = stats.count;
            last_dirty_area = stats.area;
            last_dirty_full = full;

            if (full && s_i2c_dma_enabled) {
                if (!s_i2c_dma_busy) {
                    canvas_to_ssd1306_buffer(canvas, display.buffer);
                    if (display.refresh_page_dma()) {
                        s_i2c_dma_start_ms = ms;
                        refreshed = true;
                    } else {
                        display.refresh_page();
                        refreshed = true;
                    }
                }
            } else if (!s_i2c_dma_busy) {
                const int n = canvas.dirty_count();
                for (int i = 0; i < n; ++i) {
                    const auto dr = canvas.dirty_rect_at(i);
                    const int x0 = dr.x;
                    const int y0 = dr.y;
                    const int x1 = dr.x + dr.w;
                    const int y1 = dr.y + dr.h;
                    canvas_to_ssd1306_partial(canvas, display.buffer, x0, y0, x1, y1);
                    refresh_area(display, x0, y0, x1, y1);
                }
                refreshed = true;
            }

            if (refreshed) {
                canvas.clear_dirty();
                state.fps.update(state.tick);
            }
        }
    }

    for (;;) {
        int32_t step = Encoder_GetStep();
        (void)step;
        // if (step != 0) out::println<"step = {}">(console, step);
        HAL_Delay(10);
    }
}
