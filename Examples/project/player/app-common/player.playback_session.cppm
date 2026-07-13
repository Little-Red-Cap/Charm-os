module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

export module player.playback_session;

import audio.eq;
import audio.player;
import charm.system.clock;
import player.fixed_string;
import player.playback;

export namespace player {
    enum class PlaybackResumeMode : std::uint8_t {
        Stopped,
        Playing,
        Paused,
    };

    enum class PlaybackMode : std::uint8_t {
        Sequential,
        RepeatOne,
        Shuffle,
    };

    constexpr int playback_mode_index(PlaybackMode mode) noexcept {
        return static_cast<int>(mode);
    }

    constexpr PlaybackMode playback_mode_from_index(int mode) noexcept {
        switch (mode) {
        case 1:
            return PlaybackMode::RepeatOne;
        case 2:
            return PlaybackMode::Shuffle;
        default:
            return PlaybackMode::Sequential;
        }
    }

    constexpr PlaybackMode next_playback_mode(PlaybackMode mode) noexcept {
        switch (mode) {
        case PlaybackMode::Sequential:
            return PlaybackMode::RepeatOne;
        case PlaybackMode::RepeatOne:
            return PlaybackMode::Shuffle;
        case PlaybackMode::Shuffle:
            return PlaybackMode::Sequential;
        }
        return PlaybackMode::Sequential;
    }

    constexpr const char* playback_mode_text(PlaybackMode mode) noexcept {
        switch (mode) {
        case PlaybackMode::RepeatOne:
            return "Single";
        case PlaybackMode::Shuffle:
            return "Shuffle";
        case PlaybackMode::Sequential:
        default:
            return "Order";
        }
    }

    struct PlaybackTrack {
        FixedString<260> path{};
        bool ready{false};
    };

    struct PlaybackSessionSnapshot {
        int track_index{-1};
        int track_count{0};
        int queue_position{-1};
        int queue_count{0};
        std::uint32_t queue_generation{1};
        std::uint32_t shuffle_seed{0};
        PlaybackMode mode{PlaybackMode::Sequential};
        bool track_ready{false};
        bool playing{false};
        bool paused{false};
        const char* track_path{nullptr};
    };

    template <std::size_t Capacity>
    class PlaybackSession {
    public:
        static_assert(Capacity > 0);

        PlaybackEngine& engine() noexcept { return playback_; }
        const PlaybackEngine& engine() const noexcept { return playback_; }

        void bind_clock(charm::system::Clock& clock) noexcept {
            playback_.bind_clock(clock);
        }

        void bind_player(audio::AudioPlayer& player) noexcept {
            playback_.set_player(player);
        }

        void set_player(audio::AudioPlayer& player) noexcept {
            bind_player(player);
        }

        void set_track_path(const char* path) noexcept {
            playback_.set_track_path(path);
        }

        void set_track_ready(bool ready) noexcept {
            playback_.set_track_ready(ready);
        }

        [[nodiscard]] const char* track_path() const noexcept {
            return playback_.track_path();
        }

        [[nodiscard]] bool track_ready() const noexcept {
            return playback_.track_ready();
        }

        [[nodiscard]] bool playing() const noexcept {
            return playback_.playing();
        }

        [[nodiscard]] bool paused() const noexcept {
            return playback_.paused();
        }

        [[nodiscard]] bool duration_ready() const noexcept {
            return playback_.duration_ready();
        }

        [[nodiscard]] int duration_sec() const noexcept {
            return playback_.duration_sec();
        }

        [[nodiscard]] int resolved_duration_sec() const noexcept {
            return playback_.track_ready() ? playback_.duration_sec() : 0;
        }

        [[nodiscard]] int current_sec() const noexcept {
            return playback_.current_sec();
        }

        [[nodiscard]] int volume_percent() const noexcept {
            return playback_.volume_percent();
        }

        bool snapshot(audio::PlayerSnapshot& out) {
            return playback_.snapshot(out);
        }

        void reset_duration() noexcept {
            playback_.reset_duration();
        }

        bool update_duration_from_player() {
            return playback_.update_duration_from_player();
        }

        void set_duration_from_probe(int seconds) noexcept {
            playback_.set_duration_from_probe(seconds);
        }

        void set_duration_from_probe_preserving_current(int seconds) noexcept {
            const int current = playback_.current_sec();
            playback_.set_duration_from_probe(seconds);
            playback_.set_current_sec(current);
        }

        ProgressUpdate update_progress() {
            return playback_.update_progress();
        }

        void set_current_sec(int seconds) noexcept {
            playback_.set_current_sec(seconds);
        }

        [[nodiscard]] bool is_seek_ready() const {
            return playback_.is_seek_ready();
        }

        bool request_seek(int target_sec, FixedString<128>& out_status) {
            return playback_.request_seek(target_sec, out_status);
        }

        bool set_volume(int percent, FixedString<128>& out_status) {
            return playback_.set_volume(percent, out_status);
        }

        bool set_eq(const audio::EqConfig& eq, FixedString<128>& out_status) {
            return playback_.set_eq(eq, out_status);
        }

        bool set_dc_block(bool enabled, FixedString<128>& out_status) {
            return playback_.set_dc_block(enabled, out_status);
        }

        bool set_soft_clip(bool enabled, int threshold_percent, FixedString<128>& out_status) {
            return playback_.set_soft_clip(enabled, threshold_percent, out_status);
        }

        bool apply_action(PlaybackAction action, int seek_sec, FixedString<128>& out_status) {
            return playback_.apply_action(action, seek_sec, out_status);
        }

        bool start_playback(FixedString<128>& out_status) {
            return playback_.start_playback(out_status);
        }

        bool pause_playback(FixedString<128>& out_status) {
            return playback_.pause_playback(out_status);
        }

        bool resume_playback(FixedString<128>& out_status) {
            return playback_.resume_playback(out_status);
        }

        void stop_playback() {
            playback_.stop_playback();
        }

        [[nodiscard]] bool add_track(const char* path, bool ready = true) noexcept {
            if (!path || !*path || track_count_ >= Capacity) {
                return false;
            }
            tracks_[track_count_].path.assign(path);
            tracks_[track_count_].ready = ready;
            ++track_count_;
            reset_queue();
            if (track_index_ < 0) {
                (void)load_track_index(0);
            }
            return true;
        }

        [[nodiscard]] bool set_track_catalog_size(int count) noexcept {
            if (count < 0) {
                return false;
            }
            const auto next = static_cast<std::size_t>(count);
            if (next > Capacity) {
                return false;
            }
            if (next == track_count_) {
                return true;
            }
            if (next < track_count_) {
                for (std::size_t i = next; i < track_count_; ++i) {
                    tracks_[i] = PlaybackTrack{};
                }
                if (track_index_ >= static_cast<int>(next)) {
                    clear_current_playback_state();
                }
            } else {
                for (std::size_t i = track_count_; i < next; ++i) {
                    tracks_[i] = PlaybackTrack{};
                }
            }
            track_count_ = next;
            reset_queue();
            return true;
        }

        [[nodiscard]] bool set_track_slot(int index,
                                          const char* path,
                                          bool ready = true) noexcept {
            if (index < 0 || index >= static_cast<int>(Capacity)) {
                return false;
            }
            if (index >= available_track_count()
                && !set_track_catalog_size(index + 1)) {
                return false;
            }
            auto& track = tracks_[static_cast<std::size_t>(index)];
            const std::string_view next_path = path ? std::string_view(path) : std::string_view{};
            const bool next_ready = ready && !next_path.empty();
            if (track.path.view() == next_path && track.ready == next_ready) {
                return true;
            }
            track.path.assign(next_path);
            track.ready = next_ready;
            if (track_index_ == index) {
                if (track.path.empty()) {
                    clear_current_playback_state();
                } else {
                    bind_current_track(track);
                }
            }
            return true;
        }

        void clear_tracks() noexcept {
            track_count_ = 0;
            clear_current_playback_state();
            reset_queue();
        }

        void clear_loaded_track() noexcept {
            clear_current_playback_state();
        }

        void reset_queue() noexcept {
            queue_count_ = 0;
            queue_position_ = -1;
            mark_queue_changed();
        }

        [[nodiscard]] int available_track_count() const noexcept {
            return static_cast<int>(track_count_);
        }

        [[nodiscard]] int current_track_index() const noexcept {
            return track_index_;
        }

        [[nodiscard]] PlaybackMode playback_mode() const noexcept {
            return mode_;
        }

        void set_playback_mode(PlaybackMode mode) noexcept {
            mode_ = mode;
        }

        [[nodiscard]] std::uint32_t shuffle_seed() const noexcept {
            return shuffle_seed_;
        }

        void set_shuffle_seed(std::uint32_t seed) noexcept {
            shuffle_seed_ = seed ? seed : 0xA341316Cu;
        }

        PlaybackMode cycle_playback_mode() noexcept {
            mode_ = next_playback_mode(mode_);
            return mode_;
        }

        [[nodiscard]] PlaybackResumeMode current_resume_mode() const noexcept {
            if (playback_.playing()) {
                return PlaybackResumeMode::Playing;
            }
            if (playback_.paused()) {
                return PlaybackResumeMode::Paused;
            }
            return PlaybackResumeMode::Stopped;
        }

        [[nodiscard]] bool valid_track_index(int index) const noexcept {
            return index >= 0 && index < available_track_count();
        }

        [[nodiscard]] int clamp_track_index(int index) const noexcept {
            const int count = available_track_count();
            if (count <= 0) {
                return -1;
            }
            if (index < 0) {
                return 0;
            }
            if (index >= count) {
                return count - 1;
            }
            return index;
        }

        [[nodiscard]] bool can_load_track_index(int index) const noexcept {
            const int clamped = clamp_track_index(index);
            if (clamped < 0) {
                return false;
            }
            return !tracks_[static_cast<std::size_t>(clamped)].path.empty();
        }

        bool load_track_index(int index, FixedString<128>* out_status = nullptr) noexcept {
            const int clamped = clamp_track_index(index);
            if (clamped < 0) {
                clear_current_playback_state();
                assign_status(out_status, "No track");
                return false;
            }

            PlaybackTrack& track = tracks_[static_cast<std::size_t>(clamped)];
            if (track.path.empty()) {
                clear_current_playback_state();
                assign_status(out_status, "No track");
                return false;
            }

            track_index_ = clamped;
            bind_current_track(track);
            sync_queue_position_to_track(track_index_);
            assign_status(out_status, track.ready ? "Ready" : "Track not ready");
            return track.ready;
        }

        bool select_track_index(int index, FixedString<128>& out_status) {
            return load_track_with_resume_mode(index, current_resume_mode(), out_status);
        }

        bool select_track_index(int index,
                                PlaybackResumeMode resume_mode,
                                FixedString<128>& out_status) {
            return load_track_with_resume_mode(index, resume_mode, out_status);
        }

        bool switch_track(int delta, FixedString<128>& out_status) {
            const int next = track_for_delta(delta);
            if (next < 0) {
                out_status.assign("No track");
                return false;
            }
            return load_track_with_resume_mode(next, current_resume_mode(), out_status);
        }

        bool next_track(FixedString<128>& out_status) {
            return switch_track(1, out_status);
        }

        bool previous_track(FixedString<128>& out_status) {
            return switch_track(-1, out_status);
        }

        bool advance_track(FixedString<128>& out_status) {
            const int next = track_for_playback_mode();
            if (next < 0) {
                out_status.assign("No track");
                return false;
            }
            return load_track_with_resume_mode(next, current_resume_mode(), out_status);
        }

        bool start(FixedString<128>& out_status) {
            return playback_.apply_action(PlaybackAction::start, 0, out_status);
        }

        bool pause(FixedString<128>& out_status) {
            return playback_.apply_action(PlaybackAction::pause, 0, out_status);
        }

        bool resume(FixedString<128>& out_status) {
            return playback_.apply_action(PlaybackAction::resume, 0, out_status);
        }

        bool stop(FixedString<128>& out_status) {
            return playback_.apply_action(PlaybackAction::stop, 0, out_status);
        }

        bool toggle(FixedString<128>& out_status) {
            return playback_.apply_action(PlaybackAction::toggle, 0, out_status);
        }

        bool seek(int seconds, FixedString<128>& out_status) {
            return playback_.apply_action(PlaybackAction::seek, seconds, out_status);
        }

        bool rebuild_full_queue(int selected_track = -1) noexcept {
            queue_count_ = 0;
            if (track_count_ == 0) {
                queue_position_ = -1;
                mark_queue_changed();
                return false;
            }
            for (std::size_t i = 0; i < track_count_; ++i) {
                queue_[queue_count_++] = static_cast<int>(i);
            }
            const int selected = selected_track >= 0 ? clamp_track_index(selected_track) : track_index_;
            queue_position_ = queue_position_for_track(selected >= 0 ? selected : 0);
            mark_queue_changed();
            return true;
        }

        template <typename Order>
        bool set_queue_from_order(const Order& order, int selected_track) noexcept {
            queue_count_ = 0;
            queue_position_ = -1;
            for (const int index : order) {
                if (!valid_track_index(index)) {
                    continue;
                }
                if (queue_count_ >= Capacity) {
                    break;
                }
                const int pos = static_cast<int>(queue_count_);
                queue_[queue_count_++] = index;
                if (index == selected_track && queue_position_ < 0) {
                    queue_position_ = pos;
                }
            }
            if (queue_count_ == 0 || queue_position_ < 0) {
                queue_count_ = 0;
                queue_position_ = -1;
                mark_queue_changed();
                return false;
            }
            mark_queue_changed();
            return true;
        }

        [[nodiscard]] int queue_count() const noexcept {
            return static_cast<int>(queue_count_);
        }

        [[nodiscard]] int queue_position() const noexcept {
            return queue_position_;
        }

        [[nodiscard]] std::uint32_t queue_generation() const noexcept {
            return queue_generation_;
        }

        [[nodiscard]] int queue_track_at(int position) const noexcept {
            if (position < 0 || position >= static_cast<int>(queue_count_)) {
                return -1;
            }
            return queue_[static_cast<std::size_t>(position)];
        }

        [[nodiscard]] int queue_position_for_track_index(int index) const noexcept {
            return queue_position_for_track(index);
        }

        void sync_queue_position_to_track_index(int index) noexcept {
            sync_queue_position_to_track(index);
        }

        [[nodiscard]] bool queue_ready() const noexcept {
            return queue_valid();
        }

        bool ensure_queue_ready() noexcept {
            return ensure_queue();
        }

        [[nodiscard]] int track_for_queue_delta(int delta) noexcept {
            return track_for_delta(delta);
        }

        [[nodiscard]] int random_queue_track() noexcept {
            return random_track_from_queue();
        }

        [[nodiscard]] int resolve_track_for_playback_mode() noexcept {
            return track_for_playback_mode();
        }

        PlaybackSessionSnapshot snapshot() const noexcept {
            return PlaybackSessionSnapshot{
                .track_index = track_index_,
                .track_count = available_track_count(),
                .queue_position = queue_position_,
                .queue_count = static_cast<int>(queue_count_),
                .queue_generation = queue_generation_,
                .shuffle_seed = shuffle_seed_,
                .mode = mode_,
                .track_ready = playback_.track_ready(),
                .playing = playback_.playing(),
                .paused = playback_.paused(),
                .track_path = playback_.track_path(),
            };
        }

    private:
        static void assign_status(FixedString<128>* status, const char* text) noexcept {
            if (status) {
                status->assign(text);
            }
        }

        void clear_current_playback_state() noexcept {
            track_index_ = -1;
            playback_.set_track_path(nullptr);
            playback_.set_track_ready(false);
            playback_.reset_duration();
            playback_.set_current_sec(0);
            queue_position_ = -1;
        }

        void bind_current_track(const PlaybackTrack& track) noexcept {
            playback_.set_track_path(track.path.empty() ? nullptr : track.path.c_str());
            playback_.set_track_ready(track.ready);
            playback_.reset_duration();
            playback_.set_current_sec(0);
        }

        void mark_queue_changed() noexcept {
            ++queue_generation_;
            if (queue_generation_ == 0) {
                queue_generation_ = 1;
            }
        }

        [[nodiscard]] int queue_position_for_track(int index) const noexcept {
            for (std::size_t i = 0; i < queue_count_; ++i) {
                if (queue_[i] == index) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        void sync_queue_position_to_track(int index) noexcept {
            queue_position_ = queue_position_for_track(index);
        }

        [[nodiscard]] bool queue_valid() const noexcept {
            if (queue_count_ == 0) {
                return false;
            }
            for (std::size_t i = 0; i < queue_count_; ++i) {
                if (!valid_track_index(queue_[i])) {
                    return false;
                }
            }
            return true;
        }

        bool ensure_queue() noexcept {
            if (queue_valid()) {
                const int pos = queue_position_for_track(track_index_);
                if (pos >= 0) {
                    queue_position_ = pos;
                    return true;
                }
            }
            return rebuild_full_queue(track_index_);
        }

        [[nodiscard]] int track_for_delta(int delta) noexcept {
            if (!ensure_queue()) {
                return -1;
            }
            const int count = static_cast<int>(queue_count_);
            if (count <= 0) {
                return -1;
            }
            int pos = queue_position_for_track(track_index_);
            if (pos < 0) {
                pos = queue_position_;
            }
            if (pos < 0 || pos >= count) {
                pos = 0;
            }
            pos += delta;
            while (pos < 0) {
                pos += count;
            }
            while (pos >= count) {
                pos -= count;
            }
            return queue_[static_cast<std::size_t>(pos)];
        }

        [[nodiscard]] bool track_playable(int index) const noexcept {
            if (!valid_track_index(index)) {
                return false;
            }
            const auto& track = tracks_[static_cast<std::size_t>(index)];
            return track.ready && !track.path.empty();
        }

        [[nodiscard]] int next_playable_track_from_queue(int start_pos) const noexcept {
            const int count = static_cast<int>(queue_count_);
            for (int pos = start_pos; pos < count; ++pos) {
                const int index = queue_[static_cast<std::size_t>(pos)];
                if (track_playable(index)) {
                    return index;
                }
            }
            return -1;
        }

        [[nodiscard]] int playable_queue_count() const noexcept {
            int out = 0;
            for (std::size_t i = 0; i < queue_count_; ++i) {
                if (track_playable(queue_[i])) {
                    ++out;
                }
            }
            return out;
        }

        [[nodiscard]] int random_track_from_queue() noexcept {
            if (!ensure_queue()) {
                return -1;
            }
            const int count = static_cast<int>(queue_count_);
            if (count <= 0) {
                return -1;
            }
            const int playable_count = playable_queue_count();
            if (playable_count <= 0) {
                return -1;
            }
            if (playable_count <= 1) {
                return next_playable_track_from_queue(0);
            }

            const int current_pos = queue_position_for_track(track_index_);
            int next_pos = current_pos >= 0 ? current_pos : queue_position_;
            for (int i = 0; i < count * 2; ++i) {
                next_pos = static_cast<int>(next_shuffle() % static_cast<std::uint32_t>(count));
                const int candidate = queue_[static_cast<std::size_t>(next_pos)];
                if (next_pos != current_pos && track_playable(candidate)) {
                    return candidate;
                }
            }
            for (int offset = 1; offset <= count; ++offset) {
                const int pos = ((current_pos >= 0 ? current_pos : 0) + offset) % count;
                const int candidate = queue_[static_cast<std::size_t>(pos)];
                if (track_playable(candidate)) {
                    return candidate;
                }
            }
            return -1;
        }

        [[nodiscard]] int track_for_playback_mode() noexcept {
            switch (mode_) {
            case PlaybackMode::RepeatOne:
                return track_playable(track_index_) ? track_index_ : -1;
            case PlaybackMode::Shuffle:
                return random_track_from_queue();
            case PlaybackMode::Sequential:
            default:
                return next_track_from_queue();
            }
        }

        [[nodiscard]] int next_track_from_queue() noexcept {
            if (!ensure_queue()) {
                return -1;
            }
            const int count = static_cast<int>(queue_count_);
            if (count <= 0) {
                return -1;
            }
            int pos = queue_position_for_track(track_index_);
            if (pos < 0) {
                pos = queue_position_;
            }
            if (pos < 0) {
                return next_playable_track_from_queue(0);
            }
            return next_playable_track_from_queue(pos + 1);
        }

        [[nodiscard]] std::uint32_t next_shuffle() noexcept {
            set_shuffle_seed(shuffle_seed_);
            std::uint32_t x = shuffle_seed_;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            shuffle_seed_ = x ? x : 0xA341316Cu;
            return shuffle_seed_;
        }

        bool restore_resume_mode(PlaybackResumeMode mode, FixedString<128>& out_status) {
            if (!playback_.track_ready()) {
                out_status.assign("Track not ready");
                return false;
            }
            if (mode == PlaybackResumeMode::Stopped) {
                out_status.assign("Ready");
                return true;
            }
            if (!start(out_status)) {
                return false;
            }
            if (mode == PlaybackResumeMode::Paused) {
                return pause(out_status);
            }
            return true;
        }

        bool load_track_with_resume_mode(int index,
                                         PlaybackResumeMode resume_mode,
                                         FixedString<128>& out_status) {
            if (!can_load_track_index(index)) {
                out_status.assign("No track");
                return false;
            }
            FixedString<128> ignored{};
            (void)stop(ignored);
            if (!load_track_index(index, &out_status)) {
                return false;
            }
            return restore_resume_mode(resume_mode, out_status);
        }

        PlaybackEngine playback_{};
        std::array<PlaybackTrack, Capacity> tracks_{};
        std::array<int, Capacity> queue_{};
        std::size_t track_count_{0};
        std::size_t queue_count_{0};
        int track_index_{-1};
        int queue_position_{-1};
        std::uint32_t queue_generation_{1};
        std::uint32_t shuffle_seed_{0xA341316Cu};
        PlaybackMode mode_{PlaybackMode::Sequential};
    };
}
