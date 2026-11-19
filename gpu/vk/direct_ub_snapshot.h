#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace mkwvk::direct_ub {

// Unset: snapshot only a binding that aliases CPU-writable guest memory.
// 0: keep the old direct binding (diagnosis); nonzero: snapshot every UBO.
enum class Mode : int8_t { automatic = -1, disabled = 0, forced = 1 };

inline Mode mode(const char* value) {
    if (!value) return Mode::automatic;
    return std::atoi(value) != 0 ? Mode::forced : Mode::disabled;
}

inline bool should_snapshot(Mode selected, bool direct) {
    return selected == Mode::forced || (selected == Mode::automatic && direct);
}

template <class Buffer, size_t Capacity = 4096>
class Cache {
    static_assert(Capacity && !(Capacity & (Capacity - 1)), "Capacity must be a power of two");

    struct Entry {
        uint64_t serial = 0;
        uint64_t address = 0;
        uint64_t size = 0;
        Buffer buffer{};
        uint64_t offset = 0;
    };

    std::vector<Entry> entries_;
    uint64_t serial_ = 0;
    uint64_t generation_ = ~0ull;
    uint64_t capture_view_ = ~0ull;

    static size_t slot(uint64_t address, uint64_t size) {
        uint64_t h = address ^ (size * 0x9e3779b97f4a7c15ull);
        h ^= h >> 30;
        h *= 0xbf58476d1ce4e5b9ull;
        h ^= h >> 27;
        return (size_t)h & (Capacity - 1);
    }

public:
    void reset() {
        if (entries_.empty()) entries_.resize(Capacity);
        if (++serial_ == 0) {
            for (Entry& entry : entries_) entry.serial = 0;
            serial_ = 1;
        }
    }

    bool find(uint64_t address, uint64_t size, uint64_t generation, uint64_t capture_view,
              Buffer* buffer, uint64_t* offset) const {
        if (entries_.empty() || !serial_ || generation != generation_ || capture_view != capture_view_) return false;
        size_t at = slot(address, size);
        for (size_t probe = 0; probe < Capacity; probe++, at = (at + 1) & (Capacity - 1)) {
            const Entry& entry = entries_[at];
            if (entry.serial != serial_) return false;
            if (entry.address != address || entry.size != size) continue;
            *buffer = entry.buffer;
            *offset = entry.offset;
            return true;
        }
        return false;
    }

    bool keep(uint64_t address, uint64_t size, uint64_t generation, uint64_t capture_view,
              Buffer buffer, uint64_t offset) {
        if (entries_.empty() || !serial_) reset();
        if (generation != generation_ || capture_view != capture_view_) {
            generation_ = generation;
            capture_view_ = capture_view;
            reset();
        }
        size_t at = slot(address, size);
        for (size_t probe = 0; probe < Capacity; probe++, at = (at + 1) & (Capacity - 1)) {
            Entry& entry = entries_[at];
            if (entry.serial == serial_ && (entry.address != address || entry.size != size)) continue;
            entry = {serial_, address, size, buffer, offset};
            return true;
        }
        return false;
    }
};

} // namespace mkwvk::direct_ub
