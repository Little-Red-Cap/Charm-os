module;
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>
export module demo.flip;


export namespace demo::flip {

struct Particle {
    float x{0.0f};
    float y{0.0f};
    float vx{0.0f};
    float vy{0.0f};
};

class FlipDemo {
public:
    void init() {
        if (win_) return;
        const int win_w = grid_w * cell_size;
        const int win_h = grid_h * cell_size;
        win_ = SDL_CreateWindow("Charm-ink FLIP", win_w, win_h, SDL_WINDOW_RESIZABLE);
        if (!win_) return;
        ren_ = SDL_CreateRenderer(win_, nullptr);
        if (!ren_) return;
        window_id_ = SDL_GetWindowID(win_);
        SDL_RaiseWindow(win_);
        reset_particles();
    }

    void shutdown() {
        if (ren_) { SDL_DestroyRenderer(ren_); ren_ = nullptr; }
        if (win_) { SDL_DestroyWindow(win_); win_ = nullptr; }
    }

    ~FlipDemo() { shutdown(); }

    std::uint32_t window_id() const noexcept { return window_id_; }

    void handle_event(const SDL_Event& e) noexcept {
        if (!win_) return;
        const std::uint32_t eid = event_window_id(e);
        const bool is_key = (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP);
        if (!is_key && eid != 0 && eid != window_id_) return;
        if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            want_close_ = true;
            return;
        }
        if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {
            const bool down = (e.type == SDL_EVENT_KEY_DOWN);
            switch (e.key.scancode) {
                case SDL_SCANCODE_LEFT:  key_impulse_[0] = down; break;
                case SDL_SCANCODE_RIGHT: key_impulse_[1] = down; break;
                case SDL_SCANCODE_UP:    key_impulse_[2] = down; break;
                case SDL_SCANCODE_DOWN:  key_impulse_[3] = down; break;
                case SDL_SCANCODE_J:     key_grav_[0] = down; break;
                case SDL_SCANCODE_L:     key_grav_[1] = down; break;
                case SDL_SCANCODE_I:     key_grav_[2] = down; break;
                case SDL_SCANCODE_K:     key_grav_[3] = down; break;
                case SDL_SCANCODE_A:     key_move_[0] = down; break;
                case SDL_SCANCODE_D:     key_move_[1] = down; break;
                case SDL_SCANCODE_W:     key_move_[2] = down; break;
                case SDL_SCANCODE_S:     key_move_[3] = down; break;
                case SDL_SCANCODE_R:
                    if (down) reset_particles();
                    break;
                case SDL_SCANCODE_1: if (down) param_select_ = 0; break;
                case SDL_SCANCODE_2: if (down) param_select_ = 1; break;
                case SDL_SCANCODE_3: if (down) param_select_ = 2; break;
                case SDL_SCANCODE_4: if (down) param_select_ = 3; break;
                case SDL_SCANCODE_5: if (down) param_select_ = 4; break;
                case SDL_SCANCODE_LEFTBRACKET:
                    if (down) adjust_param(-1.0f);
                    break;
                case SDL_SCANCODE_RIGHTBRACKET:
                    if (down) adjust_param(1.0f);
                    break;
                default: break;
            }
        }
    }

    bool want_close() const noexcept { return want_close_; }

    void update(std::uint32_t dt_ms) {
        if (!win_ || !ren_) return;
        const float dt = std::min(0.033f, dt_ms * 0.001f);

        update_gravity(dt);
        apply_keyboard_impulse(dt);
        particles_to_grid();
        add_gravity(dt);
        project_pressure();
        grid_to_particles(dt);
        advect_particles(dt);
    }

    void render() {
        if (!win_ || !ren_) return;
        SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
        SDL_RenderClear(ren_);
        draw_container();
        draw_waterline();
        SDL_SetRenderDrawColor(ren_, 255, 255, 255, 255);
        for (const auto& p : particles_) {
            const int px = (int)(p.x * cell_size);
            const int py = (int)(p.y * cell_size);
            SDL_RenderPoint(ren_, (float)px, (float)py);
        }
        const float cx = cursor_x_ * cell_size;
        const float cy = cursor_y_ * cell_size;
        SDL_RenderLine(ren_, cx - 6.0f, cy, cx + 6.0f, cy);
        SDL_RenderLine(ren_, cx, cy - 6.0f, cx, cy + 6.0f);
        draw_panel();
        SDL_RenderPresent(ren_);
    }

private:
    static constexpr int grid_w = 64;
    static constexpr int grid_h = 64;
    static constexpr int cell_size = 4;
    static constexpr float gravity_mag_default = 18.0f;
    static constexpr float flip_ratio_default = 0.95f;

    SDL_Window* win_{nullptr};
    SDL_Renderer* ren_{nullptr};
    std::uint32_t window_id_{0};
    bool want_close_{false};

    std::vector<Particle> particles_;
    std::vector<float> u_, v_;
    std::vector<float> u_prev_, v_prev_;
    std::vector<float> u_w_, v_w_;
    std::vector<float> pressure_;
    std::vector<float> divergence_;

    float cursor_x_{grid_w * 0.5f};
    float cursor_y_{grid_h * 0.5f};
    float grav_dir_x_{0.0f};
    float grav_dir_y_{1.0f};
    float grav_vel_x_{0.0f};
    float grav_vel_y_{0.0f};
    float gravity_mag_{gravity_mag_default};
    float flip_ratio_{flip_ratio_default};
    float tilt_stiffness_{8.0f};
    float tilt_damping_{4.5f};
    float bounce_{0.25f};
    int param_select_{0};
    std::array<float, grid_w> surface_y_{};
    bool key_impulse_[4]{false, false, false, false};
    bool key_move_[4]{false, false, false, false};
    bool key_grav_[4]{false, false, false, false};

    static std::uint32_t event_window_id(const SDL_Event& e) noexcept {
        switch (e.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                return e.window.windowID;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                return e.key.windowID;
            default:
                return 0;
        }
    }

    void reset_particles() {
        particles_.clear();
        particles_.reserve(6000);
        for (int y = 10; y < 40; ++y) {
            for (int x = 10; x < 40; ++x) {
                Particle p;
                p.x = x + 0.5f;
                p.y = y + 0.5f;
                particles_.push_back(p);
            }
        }
        const int u_count = (grid_w + 1) * grid_h;
        const int v_count = grid_w * (grid_h + 1);
        u_.assign(u_count, 0.0f);
        v_.assign(v_count, 0.0f);
        u_prev_.assign(u_count, 0.0f);
        v_prev_.assign(v_count, 0.0f);
        u_w_.assign(u_count, 0.0f);
        v_w_.assign(v_count, 0.0f);
        pressure_.assign(grid_w * grid_h, 0.0f);
        divergence_.assign(grid_w * grid_h, 0.0f);
        surface_y_.fill(-1.0f);
    }

    void apply_keyboard_impulse(float dt) {
        const float move_speed = 18.0f;
        if (key_move_[0]) cursor_x_ -= move_speed * dt;
        if (key_move_[1]) cursor_x_ += move_speed * dt;
        if (key_move_[2]) cursor_y_ -= move_speed * dt;
        if (key_move_[3]) cursor_y_ += move_speed * dt;
        cursor_x_ = std::clamp(cursor_x_, 2.0f, (float)grid_w - 3.0f);
        cursor_y_ = std::clamp(cursor_y_, 2.0f, (float)grid_h - 3.0f);

        float dvx = 0.0f;
        float dvy = 0.0f;
        const float impulse = 6.0f;
        if (key_impulse_[0]) dvx -= impulse;
        if (key_impulse_[1]) dvx += impulse;
        if (key_impulse_[2]) dvy -= impulse;
        if (key_impulse_[3]) dvy += impulse;
        if (dvx == 0.0f && dvy == 0.0f) return;

        const float radius = 3.0f;
        const float r2 = radius * radius;
        for (auto& p : particles_) {
            const float dx = p.x - cursor_x_;
            const float dy = p.y - cursor_y_;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= r2) {
                p.vx += dvx;
                p.vy += dvy;
            }
        }
    }

    void particles_to_grid() {
        std::fill(u_.begin(), u_.end(), 0.0f);
        std::fill(v_.begin(), v_.end(), 0.0f);
        std::fill(u_w_.begin(), u_w_.end(), 0.0f);
        std::fill(v_w_.begin(), v_w_.end(), 0.0f);

        for (const auto& p : particles_) {
            splat_u(p);
            splat_v(p);
        }

        for (std::size_t i = 0; i < u_.size(); ++i) {
            if (u_w_[i] > 0.0f) u_[i] /= u_w_[i];
        }
        for (std::size_t i = 0; i < v_.size(); ++i) {
            if (v_w_[i] > 0.0f) v_[i] /= v_w_[i];
        }

        u_prev_ = u_;
        v_prev_ = v_;
    }

    void update_gravity(float dt) {
        float gx = 0.0f;
        float gy = 1.0f;
        if (key_grav_[0]) gx -= 1.0f;
        if (key_grav_[1]) gx += 1.0f;
        if (key_grav_[2]) gy -= 1.0f;
        if (key_grav_[3]) gy += 1.0f;
        const float glen = std::sqrt(gx * gx + gy * gy);
        gx /= glen;
        gy /= glen;

        const float ax = (gx - grav_dir_x_) * tilt_stiffness_;
        const float ay = (gy - grav_dir_y_) * tilt_stiffness_;
        grav_vel_x_ += ax * dt;
        grav_vel_y_ += ay * dt;
        const float damp = std::exp(-tilt_damping_ * dt);
        grav_vel_x_ *= damp;
        grav_vel_y_ *= damp;
        grav_dir_x_ += grav_vel_x_ * dt;
        grav_dir_y_ += grav_vel_y_ * dt;

        const float dlen = std::sqrt(grav_dir_x_ * grav_dir_x_ + grav_dir_y_ * grav_dir_y_);
        if (dlen > 0.0001f) {
            grav_dir_x_ /= dlen;
            grav_dir_y_ /= dlen;
        } else {
            grav_dir_x_ = 0.0f;
            grav_dir_y_ = 1.0f;
        }
    }

    void add_gravity(float dt) {
        const float gx = grav_dir_x_ * gravity_mag_ * dt;
        const float gy = grav_dir_y_ * gravity_mag_ * dt;
        for (int y = 0; y < grid_h + 1; ++y) {
            for (int x = 0; x < grid_w; ++x) {
                v_[x + y * grid_w] += gy;
            }
        }
        for (int y = 0; y < grid_h; ++y) {
            for (int x = 0; x < grid_w + 1; ++x) {
                u_[x + y * (grid_w + 1)] += gx;
            }
        }
    }

    void project_pressure() {
        const int w = grid_w;
        const int h = grid_h;
        const float inv_h = 1.0f;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const float du = u_[(x + 1) + y * (w + 1)] - u_[x + y * (w + 1)];
                const float dv = v_[x + (y + 1) * w] - v_[x + y * w];
                divergence_[x + y * w] = (du + dv) * inv_h;
                pressure_[x + y * w] = 0.0f;
            }
        }

        for (int iter = 0; iter < 30; ++iter) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    float p_sum = 0.0f;
                    int count = 0;
                    if (x > 0) { p_sum += pressure_[(x - 1) + y * w]; ++count; }
                    if (x < w - 1) { p_sum += pressure_[(x + 1) + y * w]; ++count; }
                    if (y > 0) { p_sum += pressure_[x + (y - 1) * w]; ++count; }
                    if (y < h - 1) { p_sum += pressure_[x + (y + 1) * w]; ++count; }
                    if (count > 0) {
                        pressure_[x + y * w] = (p_sum - divergence_[x + y * w]) / (float)count;
                    }
                }
            }
        }

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const float p = pressure_[x + y * w];
                u_[x + y * (w + 1)] -= p;
                u_[(x + 1) + y * (w + 1)] += p;
                v_[x + y * w] -= p;
                v_[x + (y + 1) * w] += p;
            }
        }

        // boundary conditions
        for (int y = 0; y < h; ++y) {
            u_[0 + y * (w + 1)] = 0.0f;
            u_[w + y * (w + 1)] = 0.0f;
        }
        for (int x = 0; x < w; ++x) {
            v_[x + 0 * w] = 0.0f;
            v_[x + h * w] = 0.0f;
        }
    }

    void grid_to_particles(float dt) {
        for (auto& p : particles_) {
            const float pic_u = sample_u(p.x, p.y);
            const float pic_v = sample_v(p.x, p.y);
            const float prev_u = sample_u_prev(p.x, p.y);
            const float prev_v = sample_v_prev(p.x, p.y);
            const float flip_u = p.vx + (pic_u - prev_u);
            const float flip_v = p.vy + (pic_v - prev_v);
            p.vx = pic_u * (1.0f - flip_ratio_) + flip_u * flip_ratio_;
            p.vy = pic_v * (1.0f - flip_ratio_) + flip_v * flip_ratio_;
            p.vx *= 0.999f;
            p.vy *= 0.999f;
        }
    }

    void advect_particles(float dt) {
        const float cx = grid_w * 0.5f;
        const float cy = grid_h * 0.5f;
        const float rx = (grid_w - 4.0f) * 0.5f;
        const float ry = (grid_h - 6.0f) * 0.5f;
        for (auto& p : particles_) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            const float nx = (p.x - cx) / rx;
            const float ny = (p.y - cy) / ry;
            const float len = std::sqrt(nx * nx + ny * ny);
            if (len > 1.0f) {
                const float k = 1.0f / len;
                p.x = cx + (p.x - cx) * k;
                p.y = cy + (p.y - cy) * k;
                float npx = (p.x - cx) / (rx * rx);
                float npy = (p.y - cy) / (ry * ry);
                const float nlen = std::sqrt(npx * npx + npy * npy);
                if (nlen > 0.0001f) {
                    npx /= nlen;
                    npy /= nlen;
                    const float vn = p.vx * npx + p.vy * npy;
                    if (vn > 0.0f) {
                        p.vx -= vn * npx;
                        p.vy -= vn * npy;
                    } else {
                        p.vx -= (1.0f + bounce_) * vn * npx;
                        p.vy -= (1.0f + bounce_) * vn * npy;
                    }
                    p.vx *= 0.98f;
                    p.vy *= 0.98f;
                }
            }
            if (p.x < 1.0f) p.x = 1.0f;
            if (p.x > grid_w - 2.0f) p.x = grid_w - 2.0f;
            if (p.y < 1.0f) p.y = 1.0f;
            if (p.y > grid_h - 2.0f) p.y = grid_h - 2.0f;
        }
    }

    void splat_u(const Particle& p) {
        const float fx = p.x;
        const float fy = p.y - 0.5f;
        const int i0 = (int)std::floor(fx);
        const int j0 = (int)std::floor(fy);
        for (int j = 0; j <= 1; ++j) {
            for (int i = 0; i <= 1; ++i) {
                const int x = i0 + i;
                const int y = j0 + j;
                if (x < 0 || x > grid_w || y < 0 || y >= grid_h) continue;
                const float wx = 1.0f - std::abs(fx - x);
                const float wy = 1.0f - std::abs(fy - y);
                const float w = wx * wy;
                const int idx = x + y * (grid_w + 1);
                u_[idx] += p.vx * w;
                u_w_[idx] += w;
            }
        }
    }

    void splat_v(const Particle& p) {
        const float fx = p.x - 0.5f;
        const float fy = p.y;
        const int i0 = (int)std::floor(fx);
        const int j0 = (int)std::floor(fy);
        for (int j = 0; j <= 1; ++j) {
            for (int i = 0; i <= 1; ++i) {
                const int x = i0 + i;
                const int y = j0 + j;
                if (x < 0 || x >= grid_w || y < 0 || y > grid_h) continue;
                const float wx = 1.0f - std::abs(fx - x);
                const float wy = 1.0f - std::abs(fy - y);
                const float w = wx * wy;
                const int idx = x + y * grid_w;
                v_[idx] += p.vy * w;
                v_w_[idx] += w;
            }
        }
    }

    float sample_u(float x, float y) const {
        return sample_grid(u_, x, y - 0.5f, grid_w + 1, grid_h);
    }

    float sample_v(float x, float y) const {
        return sample_grid(v_, x - 0.5f, y, grid_w, grid_h + 1);
    }

    float sample_u_prev(float x, float y) const {
        return sample_grid(u_prev_, x, y - 0.5f, grid_w + 1, grid_h);
    }

    float sample_v_prev(float x, float y) const {
        return sample_grid(v_prev_, x - 0.5f, y, grid_w, grid_h + 1);
    }

    float sample_grid(const std::vector<float>& grid, float fx, float fy, int w, int h) const {
        int x0 = (int)std::floor(fx);
        int y0 = (int)std::floor(fy);
        const float tx = fx - x0;
        const float ty = fy - y0;
        x0 = std::clamp(x0, 0, w - 1);
        y0 = std::clamp(y0, 0, h - 1);
        const int x1 = std::min(x0 + 1, w - 1);
        const int y1 = std::min(y0 + 1, h - 1);

        const float v00 = grid[x0 + y0 * w];
        const float v10 = grid[x1 + y0 * w];
        const float v01 = grid[x0 + y1 * w];
        const float v11 = grid[x1 + y1 * w];

        const float a = v00 * (1.0f - tx) + v10 * tx;
        const float b = v01 * (1.0f - tx) + v11 * tx;
        return a * (1.0f - ty) + b * ty;
    }

    void draw_container() {
        const float cx = grid_w * 0.5f * cell_size;
        const float cy = grid_h * 0.5f * cell_size;
        const float rx = (grid_w - 4.0f) * 0.5f * cell_size;
        const float ry = (grid_h - 6.0f) * 0.5f * cell_size;
        SDL_SetRenderDrawColor(ren_, 64, 64, 64, 255);
        constexpr int segments = 64;
        float prev_x = cx + rx;
        float prev_y = cy;
        for (int i = 1; i <= segments; ++i) {
            const float t = (float)i / (float)segments;
            const float ang = t * 6.2831853f;
            const float x = cx + std::cos(ang) * rx;
            const float y = cy + std::sin(ang) * ry;
            SDL_RenderLine(ren_, prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
        }
    }

    void draw_waterline() {
        surface_y_.fill(-1.0f);
        for (const auto& p : particles_) {
            const int ix = (int)std::clamp(p.x, 0.0f, (float)(grid_w - 1));
            float& y = surface_y_[ix];
            if (y < 0.0f || p.y < y) y = p.y;
        }
        SDL_SetRenderDrawColor(ren_, 120, 180, 255, 255);
        float prev_x = -1.0f;
        float prev_y = -1.0f;
        for (int x = 0; x < grid_w; ++x) {
            const float sy = surface_y_[x];
            if (sy < 0.0f) {
                prev_x = -1.0f;
                prev_y = -1.0f;
                continue;
            }
            const float px = (x + 0.5f) * cell_size;
            const float py = sy * cell_size;
            if (prev_x >= 0.0f) {
                SDL_RenderLine(ren_, prev_x, prev_y, px, py);
            }
            prev_x = px;
            prev_y = py;
        }
    }

    void draw_panel() {
        int y = 6;
        const int x = 6;
        char buf[128]{};
        SDL_SetRenderDrawColor(ren_, 200, 200, 200, 255);
        SDL_RenderDebugText(ren_, x, y, "FLIP Panel [1-5 select, [] adjust]");
        y += 14;
        std::snprintf(buf, sizeof(buf), "%c Flip Ratio: %.2f", param_select_ == 0 ? '>' : ' ', flip_ratio_);
        SDL_RenderDebugText(ren_, x, y, buf);
        y += 12;
        std::snprintf(buf, sizeof(buf), "%c Gravity: %.2f", param_select_ == 1 ? '>' : ' ', gravity_mag_);
        SDL_RenderDebugText(ren_, x, y, buf);
        y += 12;
        std::snprintf(buf, sizeof(buf), "%c Tilt Stiff: %.2f", param_select_ == 2 ? '>' : ' ', tilt_stiffness_);
        SDL_RenderDebugText(ren_, x, y, buf);
        y += 12;
        std::snprintf(buf, sizeof(buf), "%c Tilt Damp: %.2f", param_select_ == 3 ? '>' : ' ', tilt_damping_);
        SDL_RenderDebugText(ren_, x, y, buf);
        y += 12;
        std::snprintf(buf, sizeof(buf), "%c Bounce: %.2f", param_select_ == 4 ? '>' : ' ', bounce_);
        SDL_RenderDebugText(ren_, x, y, buf);
        y += 12;
        std::snprintf(buf, sizeof(buf), "Tilt: (%.2f, %.2f)", grav_dir_x_, grav_dir_y_);
        SDL_RenderDebugText(ren_, x, y, buf);
    }

    void adjust_param(float dir) {
        if (param_select_ == 0) {
            flip_ratio_ = std::clamp(flip_ratio_ + dir * 0.02f, 0.0f, 1.0f);
            return;
        }
        if (param_select_ == 1) {
            gravity_mag_ = std::clamp(gravity_mag_ + dir * 1.0f, 1.0f, 40.0f);
            return;
        }
        if (param_select_ == 2) {
            tilt_stiffness_ = std::clamp(tilt_stiffness_ + dir * 0.5f, 0.5f, 20.0f);
            return;
        }
        if (param_select_ == 3) {
            tilt_damping_ = std::clamp(tilt_damping_ + dir * 0.5f, 0.5f, 20.0f);
            return;
        }
        if (param_select_ == 4) {
            bounce_ = std::clamp(bounce_ + dir * 0.05f, 0.0f, 0.8f);
            return;
        }
    }
};

} // namespace demo::flip
