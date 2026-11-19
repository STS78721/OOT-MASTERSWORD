// Game- and GPU-free regression for direct uniform-buffer snapshot policy and reuse.
//   clang++ -std=c++20 -O2 -Wall -Wextra -Werror tests/direct_ub_snapshot_test.cpp -o direct_ub_snapshot_test
#include <cstdint>

#include "../gpu/vk/direct_ub_snapshot.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main() {
    using mkwvk::direct_ub::Mode;
    using mkwvk::direct_ub::mode;
    using mkwvk::direct_ub::should_snapshot;

    CHECK(mode(nullptr) == Mode::automatic);
    CHECK(mode("0") == Mode::disabled);
    CHECK(mode("1") == Mode::forced);
    CHECK(should_snapshot(Mode::automatic, true));
    CHECK(!should_snapshot(Mode::automatic, false));
    CHECK(!should_snapshot(Mode::disabled, true));
    CHECK(should_snapshot(Mode::forced, false));

    mkwvk::direct_ub::Cache<uintptr_t, 4> cache;
    uintptr_t buffer = 0;
    uint64_t offset = 0;
    CHECK(!cache.find(0x1000, 64, 1, 10, &buffer, &offset));
    cache.reset();
    CHECK(cache.keep(0x1000, 64, 1, 10, 7, 256));
    CHECK(cache.find(0x1000, 64, 1, 10, &buffer, &offset));
    CHECK(buffer == 7 && offset == 256);
    CHECK(!cache.find(0x1000, 64, 2, 10, &buffer, &offset));
    CHECK(!cache.find(0x1000, 64, 1, 11, &buffer, &offset));
    CHECK(!cache.find(0x1000, 80, 1, 10, &buffer, &offset));
    CHECK(!cache.find(0x2000, 64, 1, 10, &buffer, &offset));

    CHECK(cache.keep(0x1000, 64, 1, 10, 9, 512));
    CHECK(cache.find(0x1000, 64, 1, 10, &buffer, &offset));
    CHECK(buffer == 9 && offset == 512);
    cache.reset();
    CHECK(!cache.find(0x1000, 64, 1, 10, &buffer, &offset));

    CHECK(cache.keep(0x1000, 64, 1, 10, 1, 16));
    CHECK(cache.keep(0x2000, 64, 1, 10, 2, 32));
    CHECK(cache.keep(0x3000, 64, 1, 10, 3, 48));
    CHECK(cache.keep(0x4000, 64, 1, 10, 4, 64));
    CHECK(cache.keep(0x3000, 64, 1, 10, 5, 80));
    CHECK(!cache.keep(0x5000, 64, 1, 10, 6, 96));
    CHECK(cache.find(0x3000, 64, 1, 10, &buffer, &offset));
    CHECK(buffer == 5 && offset == 80);

    // A new generation or submitted-memory view makes every old entry dead.
    // Reclaiming them must let the fixed-capacity cache accept new ranges.
    CHECK(cache.keep(0x5000, 64, 2, 10, 6, 96));
    CHECK(!cache.find(0x3000, 64, 1, 10, &buffer, &offset));
    CHECK(cache.find(0x5000, 64, 2, 10, &buffer, &offset));
    CHECK(cache.keep(0x6000, 64, 2, 11, 7, 112));
    CHECK(!cache.find(0x5000, 64, 2, 10, &buffer, &offset));
    CHECK(cache.find(0x6000, 64, 2, 11, &buffer, &offset));

    // Scene construction has more than 1,024 unique ranges in one epoch.
    // The production default must retain all of them without falling back to
    // a fresh copy for a range it has already seen.
    mkwvk::direct_ub::Cache<uintptr_t> dense;
    dense.reset();
    for (uint64_t i = 0; i < 1100; i++)
        CHECK(dense.keep(0x100000 + i * 0x100, 64, 3, 12, (uintptr_t)(i + 1), i * 64));
    CHECK(dense.find(0x100000 + 1099 * 0x100, 64, 3, 12, &buffer, &offset));
    CHECK(buffer == 1100 && offset == 1099 * 64);
    return 0;
}
