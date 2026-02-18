module;
#include <cstddef>

export module charm.core.pool;

import service.object_pool;

export template <typename T, std::size_t N>
using ObjectPool = service::ObjectPool<T, N>;
