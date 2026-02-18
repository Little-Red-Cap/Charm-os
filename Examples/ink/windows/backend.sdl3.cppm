//
// Created by Joho on 2025/12/30.
//

module;
#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <optional>
export module backend.sdl3;

import gui.canvas_1bpp;
import gui.core;
import input.events;
import out.api;

export namespace backend {

template<int W, int H>
class SDL3Backend {
public:
  SDL3Backend(const char* title, int windowScale = 6) {
    if (s_init_count++ == 0) {
      if (!SDL_Init(SDL_INIT_VIDEO)) {
        s_init_count--;
        throw std::runtime_error(SDL_GetError());
      }
    }

    const int winW = W * windowScale;
    const int winH = H * windowScale;

    window_ = SDL_CreateWindow(title, winW, winH, SDL_WINDOW_RESIZABLE);
    if (!window_) {
      if (--s_init_count == 0) SDL_Quit();
      throw std::runtime_error(SDL_GetError());
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
      if (--s_init_count == 0) SDL_Quit();
      throw std::runtime_error(SDL_GetError());
    }

    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
    if (!texture_) {
      SDL_DestroyRenderer(renderer_);
      SDL_DestroyWindow(window_);
      renderer_ = nullptr;
      window_ = nullptr;
      if (--s_init_count == 0) SDL_Quit();
      throw std::runtime_error(SDL_GetError());
    }

    // PC层允许一次性动态分配：展开 1bpp -> 32bpp
    pixels_.resize((size_t)W * (size_t)H);
  }

  ~SDL3Backend() {
    if (texture_)  SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_)   SDL_DestroyWindow(window_);
    if (--s_init_count == 0) SDL_Quit();
  }

  SDL3Backend(const SDL3Backend&) = delete;
  SDL3Backend& operator=(const SDL3Backend&) = delete;

  bool pump_quit() noexcept {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) return true;
      if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_ESCAPE) return true;
      }
    }
    return false;
  }


  void set_title(const char* title) noexcept {
    if (window_ && title) SDL_SetWindowTitle(window_, title);
  }

  [[nodiscard]] std::uint32_t window_id() const noexcept {
    return window_ ? SDL_GetWindowID(window_) : 0;
  }

  void update_texture(const gui::Canvas1bpp<W, H>& c, const gui::Rect* dirty = nullptr) {
    // 1bpp buffer -> ARGB8888
    // White=on, black=off.
    constexpr std::uint32_t kOn  = 0xFFFFFFFFu;
    constexpr std::uint32_t kOff = 0xFF000000u;

#if 0
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        pixels_[(size_t)y * (size_t)W + (size_t)x] = c.getPixel(x, y) ? kOn : kOff;
      }
    }
#else
    const auto src = c.bytes();              // span<const uint8_t>
    const int stride = c.strideBytes();      // bytes per row

    auto* dst = pixels_.data();              // uint32_t*, size W*H

    if (!dirty || dirty->w <= 0 || dirty->h <= 0) {
      for (int y = 0; y < H; ++y) {
        const std::uint8_t* row = src.data() + y * stride;
        std::uint32_t* out = dst + (size_t)y * (size_t)W;

        // 8 pixels per byte
        int x = 0;
        for (int bi = 0; bi < stride; ++bi) {
          const auto& p8 = kByteTo8Pixels[row[bi]];
          out[x+0] = p8[0]; out[x+1] = p8[1]; out[x+2] = p8[2]; out[x+3] = p8[3];
          out[x+4] = p8[4]; out[x+5] = p8[5]; out[x+6] = p8[6]; out[x+7] = p8[7];
          x += 8;
        }
      }
    } else {
      const int x0 = std::max<int>(0, dirty->x);
      const int y0 = std::max<int>(0, dirty->y);
      const int x1 = std::min<int>(W, dirty->x + dirty->w);
      const int y1 = std::min<int>(H, dirty->y + dirty->h);
      for (int y = y0; y < y1; ++y) {
        const std::uint8_t* row = src.data() + y * stride;
        std::uint32_t* out = dst + (size_t)y * (size_t)W;
        for (int x = x0; x < x1; ++x) {
          const int bi = x >> 3;
          const std::uint8_t mask = std::uint8_t(0x80u >> (x & 7));
          const bool on = (row[bi] & mask) != 0;
          out[x] = on ? kOn : kOff;
        }
      }
    }
#endif

#if 0
    void* texPixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(texture_, nullptr, &texPixels, &pitch)) {
      throw std::runtime_error(SDL_GetError());
    }

    auto* dstp = static_cast<std::uint8_t*>(texPixels);
    const auto* srcp = reinterpret_cast<const std::uint8_t*>(pixels_.data());
    const int srcPitch = W * 4;

    for (int y = 0; y < H; ++y) {
      std::memcpy(dstp + y * pitch, srcp + y * srcPitch, (size_t)srcPitch);
    }

    SDL_UnlockTexture(texture_);
#else
    if (dirty && dirty->w > 0 && dirty->h > 0) {
      SDL_Rect rc{dirty->x, dirty->y, dirty->w, dirty->h};
      const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(
          pixels_.data() + (size_t)rc.y * (size_t)W + (size_t)rc.x);
      if (!SDL_UpdateTexture(texture_, &rc, ptr, W * 4)) {
        throw std::runtime_error(SDL_GetError());
      }
    } else {
      if (!SDL_UpdateTexture(texture_, nullptr, pixels_.data(), W * 4)) {
        throw std::runtime_error(SDL_GetError());
      }
    }
#endif
  }

  void present(const gui::Canvas1bpp<W, H>& c, const gui::Rect* dirty = nullptr) {
    update_texture(c, dirty);
    present_frame();
  }

  void present_frame() {
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window_, &winW, &winH);

    const int scale = std::max(1, std::min(winW / W, winH / H));
    const int dstW = W * scale;
    const int dstH = H * scale;
    const int dstX = (winW - dstW) / 2;
    const int dstY = (winH - dstH) / 2;

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    SDL_FRect dstRect{(float)dstX, (float)dstY, (float)dstW, (float)dstH};
    SDL_RenderTexture(renderer_, texture_, nullptr, &dstRect);

    SDL_RenderPresent(renderer_);
  }

  static consteval auto make_lut() {
    std::array<std::array<std::uint32_t, 8>, 256> lut{};
    for (int v = 0; v < 256; ++v) {
      for (int i = 0; i < 8; ++i) {
        // bit7 -> x+0, bit0 -> x+7（与你的 Canvas setPixel 约定一致）
        const bool on = (v & (0x80 >> i)) != 0;
        lut[v][i] = on ? 0xFFFFFFFFu : 0xFF000000u;
      }
    }
    return lut;
  }
  static constexpr auto kByteTo8Pixels = make_lut();

  // 取一个事件（无事件返回 nullopt）
#if 0
  std::optional<input::Event> poll_event(std::uint32_t now_ms) noexcept {
    static int n = 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      // if (n++ < 50) out::trace<"event type={}">((unsigned)e.type);
      out::trace<"KEY event: keycode={} scancode={} down={} repeat={}">(
          (int)e.key.key, (int)e.key.scancode, (int)e.key.down, (int)e.key.repeat);


      if (e.type == SDL_EVENT_QUIT) {
        quit_ = true;
        return std::nullopt;
      }

      if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {
        const bool pressed = (e.type == SDL_EVENT_KEY_DOWN);

        using input::Key;
        std::optional<Key> k;

        switch (e.key.key) {
        case SDLK_UP:    k = Key::Up; break;
        case SDLK_DOWN:  k = Key::Down; break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: k = Key::Enter; break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE: k = Key::Back; break;
        default: break;
        }

        if (k) {
          return input::Event{
            .type = input::Type::Key,
            .key = *k,
            .pressed = pressed,
            .ms = now_ms
          };
        }
      }
    }
    return std::nullopt;
  }
#elif 0 // 输入由：SDLRawSource + input::Sampler 产生 Intent. 这里可以删了：输入彻底独立。
  std::optional<input::Event> poll_event(std::uint32_t now_ms) noexcept {
    // 1) 先处理系统事件（至少要能退出）
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) {
        quit_ = true;
        return std::nullopt;
      }
    }

    // 2) 更新输入状态并轮询键盘
    SDL_PumpEvents(); // 确保键盘状态是最新的 :contentReference[oaicite:1]{index=1}

    int nkeys = 0;
    const bool* ks = SDL_GetKeyboardState(&nkeys);
    if (!ks || nkeys <= 0) return std::nullopt;

    auto edge = [&](int sc, bool& prev, input::Key key) -> std::optional<input::Event> {
      if (sc < 0 || sc >= nkeys) return std::nullopt;
      const bool now = ks[sc];
      if (now && !prev) {
        prev = now;
        return input::Event{ .type=input::Type::Key, .key=key, .pressed=true, .ms=now_ms };
      }
      prev = now;
      return std::nullopt;
    };

    // 只产生 KeyDown（按下沿），KeyUp 目前不需要
    if (auto ev = edge(SDL_SCANCODE_UP,        prev_up_,    input::Key::Up))    return ev;
    if (auto ev = edge(SDL_SCANCODE_DOWN,      prev_down_,  input::Key::Down))  return ev;
    if (auto ev = edge(SDL_SCANCODE_RETURN,    prev_enter_, input::Key::Enter)) return ev;
    if (auto ev = edge(SDL_SCANCODE_ESCAPE,    prev_back_,  input::Key::Back))  return ev;
    if (auto ev = edge(SDL_SCANCODE_BACKSPACE, prev_back_,  input::Key::Back))  return ev;

    return std::nullopt;
  }
#endif

  bool should_quit() const noexcept { return quit_; }

private:
  SDL_Window* window_{nullptr};
  SDL_Renderer* renderer_{nullptr};
  SDL_Texture* texture_{nullptr};
  std::vector<std::uint32_t> pixels_;
  bool quit_{false};

  bool prev_up_{false}, prev_down_{false}, prev_enter_{false}, prev_back_{false};
  inline static int s_init_count = 0;

};

} // namespace backend
