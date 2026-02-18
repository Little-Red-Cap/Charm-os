module;
#include <cstdint>
export module charm.core.geometry;

export import ui.common;

export using Point = ui::PointT<std::int32_t>;
export using Size = ui::SizeT<std::int32_t>;
export using Rect = ui::RectT<std::int32_t>;
