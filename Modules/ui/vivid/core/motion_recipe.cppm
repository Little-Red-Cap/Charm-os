module;

#include <cstdint>

export module charm.ui.scene.motion_recipe;

export import charm.ui.scene.motion_plan;

export namespace ui::scene {
    enum class MotionRecipeKind : std::uint8_t {
        Cut,
        Fade,
        Slide,
        FadeSlide,
    };

    enum class MotionAxis : std::uint8_t {
        X,
        Y,
    };

    struct MotionRecipe {
        MotionRecipeKind kind{MotionRecipeKind::Cut};
        MotionAxis axis{MotionAxis::X};
        std::int16_t distance{0};
        std::uint8_t from_opacity{255};
        std::uint8_t to_opacity{255};
        std::uint32_t duration_ms{0};
    };

    constexpr const char* motion_recipe_name(MotionRecipeKind kind) noexcept {
        switch (kind) {
        case MotionRecipeKind::Cut: return "cut";
        case MotionRecipeKind::Fade: return "fade";
        case MotionRecipeKind::Slide: return "slide";
        case MotionRecipeKind::FadeSlide: return "fade_slide";
        }
        return "unknown";
    }

    constexpr MotionRecipe motion_cut() noexcept {
        return {
            .kind = MotionRecipeKind::Cut,
            .duration_ms = 0,
        };
    }

    constexpr MotionRecipe motion_fade(std::uint32_t duration_ms,
                                       std::uint8_t from_opacity = 0,
                                       std::uint8_t to_opacity = 255) noexcept {
        return {
            .kind = MotionRecipeKind::Fade,
            .from_opacity = from_opacity,
            .to_opacity = to_opacity,
            .duration_ms = duration_ms,
        };
    }

    constexpr MotionRecipe motion_slide(MotionAxis axis,
                                        std::int16_t distance,
                                        std::uint32_t duration_ms) noexcept {
        return {
            .kind = MotionRecipeKind::Slide,
            .axis = axis,
            .distance = distance,
            .from_opacity = 255,
            .to_opacity = 255,
            .duration_ms = duration_ms,
        };
    }

    constexpr MotionRecipe motion_fade_slide(MotionAxis axis,
                                             std::int16_t distance,
                                             std::uint32_t duration_ms,
                                             std::uint8_t from_opacity = 0,
                                             std::uint8_t to_opacity = 255) noexcept {
        return {
            .kind = MotionRecipeKind::FadeSlide,
            .axis = axis,
            .distance = distance,
            .from_opacity = from_opacity,
            .to_opacity = to_opacity,
            .duration_ms = duration_ms,
        };
    }

    constexpr LayerTransform motion_recipe_from_transform(const MotionRecipe& recipe) noexcept {
        LayerTransform transform{};
        transform.opacity = recipe.from_opacity;
        switch (recipe.kind) {
        case MotionRecipeKind::Cut:
            transform.opacity = recipe.to_opacity;
            return transform;
        case MotionRecipeKind::Fade:
            return transform;
        case MotionRecipeKind::Slide:
        case MotionRecipeKind::FadeSlide:
            if (recipe.axis == MotionAxis::X) {
                transform.x = recipe.distance;
            } else {
                transform.y = recipe.distance;
            }
            return transform;
        }
        return transform;
    }

    constexpr LayerTransform motion_recipe_to_transform(const MotionRecipe& recipe) noexcept {
        LayerTransform transform{};
        transform.opacity = recipe.to_opacity;
        return transform;
    }

    constexpr LayerMotionSpec make_layer_motion_spec(
        const MotionRecipe& recipe,
        LayerProfile profile,
        std::uint64_t start_ms,
        std::uint64_t now_ms) noexcept {
        return {
            .profile = profile,
            .start_ms = start_ms,
            .now_ms = now_ms,
            .duration_ms = recipe.duration_ms,
            .from = motion_recipe_from_transform(recipe),
            .to = motion_recipe_to_transform(recipe),
        };
    }

    constexpr LayerMotionFrame sample_motion_recipe(
        const MotionRecipe& recipe,
        LayerProfile profile,
        std::uint64_t start_ms,
        std::uint64_t now_ms) noexcept {
        return sample_layer_motion(make_layer_motion_spec(recipe, profile, start_ms, now_ms));
    }
}
