module;

#include <concepts>

export module media.pipeline;

import util.core;
import media.stream.types;
import media.stream.source;
import media.stream.filter;
import media.stream.sink;

export namespace media {
    template <typename T>
    concept StreamPipeline = requires(T& t, StreamSourceRef src, StreamSinkRef sink, StreamFilterRef filter) {
        { t.set_source(src) } noexcept -> std::same_as<void>;
        { t.set_sink(sink) } noexcept -> std::same_as<void>;
        { t.add_filter(filter) } noexcept -> std::same_as<Result<void>>;
        { t.start() } noexcept -> std::same_as<Result<void>>;
        { t.stop() } noexcept -> std::same_as<Result<void>>;
        { t.tick() } noexcept -> std::same_as<Result<void>>;
    };
}
