// Indirect mesh draw argument layout conversion ({count, 1, 1} ring slots) and per-barrier batch scan
// (RX_VK_MESH_CONV, vk_draw.cpp mesh_indirect_convert, vk_replay.cpp NOP_BARRIER).
//
// RX_VK_MESH_CONV:
//   0  each draw converts individually with pass end, fill of 1s, copy, and barriers
//   1  ring pre-filled with 1s once at creation; single draw copy between two full barriers
//   2  as 1, plus at each game barrier scans up to the next barrier/write/end and batch-converts
//      indirect mesh draws together before opening their pass
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mesh_conv {

constexpr uint64_t kRecord = 12;   // {x, y, z}: VkDrawMeshTasksIndirectCommandEXT
// Slots align to LCM(16, 12) = 48 bytes so every element's x lies at 0 mod 12 (y, z at 4, 8 mod 12).
constexpr uint64_t kSlotAlign = 48;
constexpr uint32_t kMaxRecords = 4096;

/// Rounded ring allocation size for n indirect command records.
constexpr uint64_t slot_bytes(uint32_t n) { return ((uint64_t)n * kRecord + kSlotAlign - 1) / kSlotAlign * kSlotAlign; }

/// Allocate the next ring slot of n records using monotonic byte offset pos.
inline uint64_t take(uint64_t& pos, uint64_t ring, uint32_t n, uint64_t* start) {
    const uint64_t bytes = slot_bytes(n);
    uint64_t off = pos % ring;
    if (off + bytes > ring) pos += ring - off, off = 0;
    if (start) *start = pos;
    pos += bytes;
    return off;
}

/// Whether the ring has not yet wrapped over the slot allocated at start.
constexpr bool still_holds(uint64_t pos, uint64_t start, uint64_t ring) { return pos <= start + ring; }

/// An indirect mesh draw discovered during barrier scan for batch conversion.
struct Item {
    const void* op = nullptr;
    uint64_t indirect = 0;
    int32_t max = 0, stride = 0;
};

/// Stream opcode classification for barrier scan termination vs pass-through.
enum class OpKind : uint8_t {
    Stop,
    Pass,
    Draw,
};

constexpr uint32_t kOpTable = 256;

struct Ops {
    uint32_t end, barrier, draw_mesh;
    OpKind kind[kOpTable];
};

/// Build O(1) opcode lookup table at compile time. Unknown or out-of-range opcodes map safely to Stop.
constexpr Ops make_ops(uint32_t end, uint32_t barrier, uint32_t draw_mesh, const uint32_t* pass, size_t npass) {
    Ops o{end, barrier, draw_mesh, {}};
    for (OpKind& k : o.kind) k = OpKind::Stop;
    for (size_t i = 0; i < npass; i++)
        if (pass[i] < kOpTable) o.kind[pass[i]] = OpKind::Pass;
    if (draw_mesh < kOpTable) o.kind[draw_mesh] = OpKind::Draw;
    return o;
}

inline OpKind kind_of(const Ops& ops, uint32_t op) { return op < kOpTable ? ops.kind[op] : OpKind::Stop; }

enum Stop { kStopEnd, kStopBarrier, kStopOp, kStopFull, kStops };

/// Scan command records starting at p (after a barrier or stop-op) for batchable indirect mesh draws.
inline Stop scan(const Ops& ops, const uint8_t* p, const uint8_t* end, std::vector<Item>& out, uint64_t most_bytes,
                 size_t most, uint32_t* stop_op, const uint8_t** at = nullptr) {
    out.clear();
    uint64_t bytes = 0;
    for (;;) {
        if (at) *at = p;
        if (p == end) return kStopEnd;
        uint32_t h[2];
        std::memcpy(h, p, sizeof h);
        if (h[0] == ops.end) return kStopEnd;
        if (h[0] == ops.barrier) return kStopBarrier;
        if (h[1] < 8) return kStopEnd;
        switch (kind_of(ops, h[0])) {
        case OpKind::Stop:
            if (stop_op) *stop_op = h[0];
            return kStopOp;
        case OpKind::Draw: {
            struct {
                int32_t x, y, z, max, stride, kind;
                uint64_t indirect, count_addr;
            } d{};
            std::memcpy(&d, p + 8, h[1] - 8 < sizeof d ? h[1] - 8 : sizeof d);
            if (!d.indirect) break;
            const uint32_t n = (uint32_t)(d.max < 1 ? 1 : d.max > (int32_t)kMaxRecords ? kMaxRecords : d.max);
            if (out.size() >= most || bytes + slot_bytes(n) > most_bytes) return kStopFull;
            bytes += slot_bytes(n);
            out.push_back({p, d.indirect, d.max, d.stride});
            break;
        }
        case OpKind::Pass: break;
        }
        p += h[1];
    }
}

/// Converted slot for one indirect mesh draw within a pre-converted barrier batch.
struct Entry {
    const void* op = nullptr;
    uint64_t off = 0, start = 0;
};

/// Per-recording-thread active batch of pre-converted indirect mesh draws.
struct Batch {
    const void* cmd = nullptr;
    std::vector<Entry> e;
    size_t next = 0;
    void clear() { cmd = nullptr, e.clear(), next = 0; }
    bool live() const { return cmd && next < e.size(); }
    /// Consume the slot for op when recording into cmd_now, skipping earlier culled/skipped entries.
    const Entry* take(const void* cmd_now, const void* op) {
        if (!cmd || cmd_now != cmd) return nullptr;
        for (size_t i = next; i < e.size(); i++)
            if (e[i].op == op) {
                next = i + 1;
                return &e[i];
            }
        return nullptr;
    }
};

} // namespace mesh_conv
