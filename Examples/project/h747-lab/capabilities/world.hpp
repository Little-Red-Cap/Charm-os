#pragma once

#include "capabilities/display.hpp"
#include "capabilities/input.hpp"
#include "capabilities/stream.hpp"
#include "capabilities/time.hpp"

#include <concepts>

namespace charm::cap {

template <class W>
concept World = requires(W& world) {
    typename W::Log;
    typename W::Clock;

    { world.log() } -> std::same_as<typename W::Log&>;
    { world.clock() } -> std::same_as<typename W::Clock&>;
} && TextSink<typename W::Log> && Clock<typename W::Clock>;

template <class W>
concept RasterDisplayWorld = World<W> && requires(W& world) {
    typename W::Display;

    { world.display() } -> std::same_as<typename W::Display&>;
    { world.framebuffer() } -> std::same_as<FrameBuffer>;
} && RasterDisplaySink<typename W::Display>;

template <class W>
concept InputWorld = World<W> && requires(W& world) {
    typename W::Input;

    { world.input() } -> std::same_as<typename W::Input&>;
} && InputSource<typename W::Input>;

template <class W>
concept RasterDisplayInputWorld = RasterDisplayWorld<W> && InputWorld<W>;

} // namespace charm::cap
