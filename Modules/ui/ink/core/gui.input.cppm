//
// Created by Joho on 2025/12/30.
//

module;

export module gui.input;

import input.raw;
import input.raw_event;
import input.intent;
import input.events;
import input.queue;
import input.encoder_decoder;
import input.sampler;
import input.raw_sampler;

export namespace gui::input {
    using ::input::AxisRaw;
    using ::input::Button;
    using ::input::DebounceCfg;
    using ::input::EncoderCfg;
    using ::input::EncoderDecoder;
    using ::input::EncoderDelta;
    using ::input::Event;
    using ::input::Intent;
    using ::input::IntentType;
    using ::input::Key;
    using ::input::PointerRaw;
    using ::input::PointerAction;
    using ::input::RawInputEvent;
    using ::input::RawInputEventType;
    using ::input::RawSource;
    using ::input::RepeatCfg;
    using ::input::RingQueue;
    using ::input::RawSampler;
    using ::input::Sampler;
    using ::input::SamplerCfg;
    using ::input::Type;
} // namespace gui::input
