module;
#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>
export module backend.sdl3;

import gui.canvas_1bpp;
import gui.core;

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

    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
    pixels_.resize(static_cast<std::size_t>(W) * static_cast<std::size_t>(H));
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
      if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) return true;
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
    constexpr std::uint32_t kOn  = 0xFFFFFFFFu;
    constexpr std::uint32_t kOff = 0xFF000000u;

    const auto src = c.bytes();
    const int stride = c.strideBytes();
    auto* dst = pixels_.data();

    if (!dirty || dirty->w <= 0 || dirty->h <= 0) {
      for (int y = 0; y < H; ++y) {
        const std::uint8_t* row = src.data() + y * stride;
        std::uint32_t* out = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(W);
        int x = 0;
        for (int bi = 0; bi < stride; ++bi) {
          const auto& p8 = kByteTo8Pixels[row[bi]];
          out[x + 0] = p8[0]; out[x + 1] = p8[1]; out[x + 2] = p8[2]; out[x + 3] = p8[3];
          out[x + 4] = p8[4]; out[x + 5] = p8[5]; out[x + 6] = p8[6]; out[x + 7] = p8[7];
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
        std::uint32_t* out = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(W);
        for (int x = x0; x < x1; ++x) {
          const int bi = x >> 3;
          const std::uint8_t mask = std::uint8_t(0x80u >> (x & 7));
          const bool on = (row[bi] & mask) != 0;
          out[x] = on ? kOn : kOff;
        }
      }
    }

    if (dirty && dirty->w > 0 && dirty->h > 0) {
      SDL_Rect rc{dirty->x, dirty->y, dirty->w, dirty->h};
      const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(
          pixels_.data() + static_cast<std::size_t>(rc.y) * static_cast<std::size_t>(W) + static_cast<std::size_t>(rc.x));
      if (!SDL_UpdateTexture(texture_, &rc, ptr, W * 4)) {
        throw std::runtime_error(SDL_GetError());
      }
    } else {
      if (!SDL_UpdateTexture(texture_, nullptr, pixels_.data(), W * 4)) {
        throw std::runtime_error(SDL_GetError());
      }
    }
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

    SDL_FRect dstRect{static_cast<float>(dstX), static_cast<float>(dstY),
                      static_cast<float>(dstW), static_cast<float>(dstH)};
    SDL_RenderTexture(renderer_, texture_, nullptr, &dstRect);
    SDL_RenderPresent(renderer_);
  }

  static consteval auto make_lut() {
    std::array<std::array<std::uint32_t, 8>, 256> lut{};
    for (int v = 0; v < 256; ++v) {
      for (int i = 0; i < 8; ++i) {
        const bool on = (v & (0x80 >> i)) != 0;
        lut[v][i] = on ? 0xFFFFFFFFu : 0xFF000000u;
      }
    }
    return lut;
  }

  static constexpr auto kByteTo8Pixels = make_lut();

private:
  SDL_Window* window_{nullptr};
  SDL_Renderer* renderer_{nullptr};
  SDL_Texture* texture_{nullptr};
  std::vector<std::uint32_t> pixels_;
  inline static int s_init_count = 0;
};

} // namespace backend
