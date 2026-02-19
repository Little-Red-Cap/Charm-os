export module media.pipeline;

import util.core;
import media.stream.types;
import media.stream.source;
import media.stream.filter;
import media.stream.sink;

export namespace media {
    struct IPipeline {
        virtual ~IPipeline() = default;

        virtual void set_source(IStreamSource* source) noexcept = 0;
        virtual void set_sink(IStreamSink* sink) noexcept = 0;
        virtual Result<void> add_filter(IStreamFilter* filter) noexcept = 0;

        virtual Result<void> start() noexcept = 0;
        virtual Result<void> stop() noexcept = 0;
        virtual Result<void> tick() noexcept = 0;
    };
}
