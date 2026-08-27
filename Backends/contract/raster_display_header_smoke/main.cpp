#include "Backends/contract/raster_display.hpp"
#include "Modules/core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <limits>

namespace relation = charm::capability;
namespace raster = charm::backend::contract::raster;

namespace {
    struct MemoryDisplay {
        std::size_t present_count{0};

        [[nodiscard]] raster::PresentResult present(const raster::SurfaceView surface,
                                                     raster::DirtyRegion dirty) noexcept {
            if (!surface.valid()) {
                return {
                    .status = raster::Status{raster::StatusCode::invalid_argument},
                };
            }
            if (dirty.empty()) {
                dirty = raster::full_region(surface);
            }
            const auto clipped = raster::clip_region(dirty, surface);
            if (clipped.empty()) {
                return {
                    .status = raster::Status{raster::StatusCode::invalid_argument},
                };
            }
            ++present_count;
            return {
                .submitted = clipped,
                .frame_index = present_count,
            };
        }
    };

    struct MissingPresent {};

    enum class ContractKey : std::uint8_t {
        raster_display,
    };
    enum class RequirementKey : std::uint8_t {
        primary,
    };
    enum class ProvisionKey : std::uint8_t {
        memory_display,
    };

    constexpr relation::Requirement<ContractKey, RequirementKey> primary_requirement{
        RequirementKey::primary, ContractKey::raster_display};
    constexpr relation::Provision<ContractKey, ProvisionKey> primary_provision{
        ProvisionKey::memory_display, ContractKey::raster_display};

    static_assert(raster::RasterDisplay::satisfied_by<MemoryDisplay>);
    static_assert(!raster::RasterDisplay::satisfied_by<MissingPresent>);
    static_assert(primary_requirement.contract == primary_provision.contract);
    static_assert(raster::bytes_per_pixel(raster::PixelFormat::rgb565) == 2U);
    static_assert(raster::bytes_per_pixel(raster::PixelFormat::rgb888) == 3U);
    static_assert(raster::bytes_per_pixel(raster::PixelFormat::argb8888) == 4U);

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        std::array<std::byte, 8U * 6U * 4U> pixels{};
        const raster::SurfaceView surface{
            .pixels = pixels,
            .width = 8,
            .height = 6,
            .stride_bytes = 8U * 4U,
            .pixel_format = raster::PixelFormat::argb8888,
        };
        const raster::SurfaceView bad_stride{
            .pixels = pixels,
            .width = 8,
            .height = 6,
            .stride_bytes = 8U,
            .pixel_format = raster::PixelFormat::argb8888,
        };
        const raster::SurfaceView truncated{
            .pixels = std::span<const std::byte>{pixels}.first(32),
            .width = 8,
            .height = 6,
            .stride_bytes = 8U * 4U,
            .pixel_format = raster::PixelFormat::argb8888,
        };
        const raster::SurfaceView oversized{
            .pixels = pixels,
            .width = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) + 1U,
            .height = 1,
            .stride_bytes = pixels.size(),
            .pixel_format = raster::PixelFormat::argb8888,
        };
        const raster::SurfaceView overflowing{
            .pixels = pixels,
            .width = 1,
            .height = std::numeric_limits<std::uint32_t>::max(),
            .stride_bytes = std::numeric_limits<std::size_t>::max(),
            .pixel_format = raster::PixelFormat::argb8888,
        };

        MemoryDisplay display{};
        const auto full = display.present(surface, {});
        const auto clipped = display.present(surface, {-2, 2, 6, 8});
        const auto outside = display.present(surface, {20, 20, 2, 2});
        const auto invalid = display.present(bad_stride, {});
        const auto short_buffer = display.present(truncated, {});
        const auto extreme_dirty = raster::clip_region(
            {std::numeric_limits<std::int32_t>::max() - 1,
             std::numeric_limits<std::int32_t>::max() - 1,
             std::numeric_limits<std::int32_t>::max(),
             std::numeric_limits<std::int32_t>::max()},
            surface);

        bool ok = true;
        ok &= expect(surface.valid(), "valid surface should satisfy stride and geometry");
        ok &= expect(!bad_stride.valid(), "short stride should reject surface");
        ok &= expect(!truncated.valid(), "short pixel span should reject surface");
        ok &= expect(!oversized.valid()
                         && raster::full_region(oversized).empty()
                         && raster::clip_region({0, 0, 1, 1}, oversized).empty(),
                     "geometry beyond dirty-region range should reject without truncation");
        ok &= expect(!overflowing.valid() && overflowing.required_size_bytes() == 0U,
                     "surface byte-size multiplication should reject overflow");
        ok &= expect(extreme_dirty.empty(), "extreme dirty arithmetic should clip without overflow");
        ok &= expect(full.is_ok() && full.submitted.width == 8 && full.submitted.height == 6,
                     "empty dirty region should submit the full surface");
        ok &= expect(clipped.is_ok()
                         && clipped.submitted.x == 0
                         && clipped.submitted.y == 2
                         && clipped.submitted.width == 4
                         && clipped.submitted.height == 4,
                     "dirty region should clip to the surface");
        ok &= expect(!outside.is_ok() && !invalid.is_ok() && !short_buffer.is_ok(),
                     "outside dirty region and invalid surfaces should fail");
        ok &= expect(display.present_count == 2U, "only valid submissions should increment frame count");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-contract-raster-display-header-smoke] ok");
    return 0;
}
