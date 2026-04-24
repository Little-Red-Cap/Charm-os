module;

#include <array>
#include <span>
#include <string_view>

export module posix.program_catalog;

import posix.program_image;
import util.core;
import util.error;

export namespace posix {
    struct ExecEntry {
        ProgramImage image{};
    };

    struct ElfMemImage {
        std::string_view name{};
        const util::u8* data{nullptr};
        util::usize size{0};
    };

    inline bool match_exec_name(std::string_view exec_name, std::string_view query) noexcept {
        if (exec_name.compare(query) == 0) return true;
        if (query.empty()) return false;
        if (exec_name.size() <= query.size()) return false;
        const auto suffix_pos = exec_name.size() - query.size();
        if (suffix_pos == 0) return false;
        if (exec_name[suffix_pos - 1] != '/') return false;
        return exec_name.substr(suffix_pos).compare(query) == 0;
    }

    template <util::usize MaxExecs>
    class ProgramCatalog {
    public:
        void reset() noexcept {
            for (auto& entry : entries_) {
                entry = {};
            }
            count_ = 0;
        }

        util::Result<void> register_registered_image(std::string_view name, ImageEntryV0 entry) noexcept {
            if (name.empty() || entry == nullptr) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < count_; ++i) {
                if (entries_[i].image.name.compare(name) == 0) {
                    return util::unexpected(util::Errc::exist);
                }
            }
            if (count_ >= MaxExecs) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            entries_[count_++] = ExecEntry{make_registered_image(name, entry)};
            return {};
        }

        util::Result<void> register_registered_image(std::string_view name, ImageEntry entry) noexcept {
            if (name.empty() || entry == nullptr) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < count_; ++i) {
                if (entries_[i].image.name.compare(name) == 0) {
                    return util::unexpected(util::Errc::exist);
                }
            }
            if (count_ >= MaxExecs) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            entries_[count_++] = ExecEntry{make_registered_image(name, entry)};
            return {};
        }

        util::Result<void> register_image(const ProgramImage& image) noexcept {
            if (image.name.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto valid = validate_program_image(image);
            if (!valid) {
                return util::unexpected(valid.error());
            }
            for (util::usize i = 0; i < count_; ++i) {
                if (entries_[i].image.name.compare(image.name) == 0) {
                    return util::unexpected(util::Errc::exist);
                }
            }
            if (count_ >= MaxExecs) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            entries_[count_++] = ExecEntry{image};
            return {};
        }

        ExecEntry* find(std::string_view name) noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (match_exec_name(entries_[i].image.name, name)) return &entries_[i];
            }
            return nullptr;
        }

        const ExecEntry* find(std::string_view name) const noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (match_exec_name(entries_[i].image.name, name)) return &entries_[i];
            }
            return nullptr;
        }

        ExecEntry* find_by_argv0(std::span<const char* const> argv) noexcept {
            if (argv.empty() || argv[0] == nullptr) return nullptr;
            return find(std::string_view{argv[0]});
        }

    private:
        std::array<ExecEntry, MaxExecs> entries_{};
        util::usize count_{0};
    };

    template <util::usize MaxEntries>
    class ElfMemRegistry {
    public:
        void reset() noexcept {
            for (auto& entry : entries_) {
                entry = {};
            }
            count_ = 0;
        }

        util::Result<void> register_image(std::string_view name, const void* data, util::usize size) noexcept {
            if (name.empty() || !data || size == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < count_; ++i) {
                if (entries_[i].name.compare(name) == 0) {
                    return util::unexpected(util::Errc::exist);
                }
            }
            if (count_ >= MaxEntries) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            entries_[count_++] = ElfMemImage{name, static_cast<const util::u8*>(data), size};
            return {};
        }

        const ElfMemImage* find(std::string_view name) const noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (entries_[i].name.compare(name) == 0) return &entries_[i];
            }
            return nullptr;
        }

    private:
        std::array<ElfMemImage, MaxEntries> entries_{};
        util::usize count_{0};
    };
}
