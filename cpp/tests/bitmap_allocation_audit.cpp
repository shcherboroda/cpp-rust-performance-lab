#if defined(LLAB_BITMAP_V6)
#include "llab/bitmap_packed_order_book.hpp"
#else
#include "llab/bitmap_ladder_order_book.hpp"
#endif

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <stdexcept>

namespace {
std::atomic<bool> count_allocations = false;
std::atomic<std::size_t> allocation_calls = 0;

void require(bool condition) {
    if (!condition)
        throw std::runtime_error("allocation audit failed");
}
} // namespace

void *operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed))
        allocation_calls.fetch_add(1, std::memory_order_relaxed);
    if (void *memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void *memory) noexcept {
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

int main() {
#if defined(LLAB_BITMAP_V6)
    using namespace llab::bitmap_packed_order_book;
#else
    using namespace llab::bitmap_ladder_order_book;
#endif
    OrderBook book(32'768, 99'873, 256);

    allocation_calls.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    for (std::uint64_t cycle = 0; cycle < 8'192; ++cycle) {
        const auto id = cycle * 3 + 1;
        const auto price = 100'000 + cycle % 128;
        book.apply({EventType::Add, id, 0, Side::Bid, price, 10});
        book.apply({EventType::Cancel, id, 0, Side::Bid, 0, 3});
        book.apply({EventType::Execute, id, 0, Side::Bid, 0, 7});
    }
    count_allocations.store(false, std::memory_order_relaxed);
    require(book.live_order_count() == 0);
    require(allocation_calls.load(std::memory_order_relaxed) == 0);
}
