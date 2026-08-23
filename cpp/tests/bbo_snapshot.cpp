#include "llab/bbo_snapshot.hpp"

#include <atomic>
#include <stdexcept>
#include <thread>

int main() {
    llab::BboSnapshotCell cell;
    if (cell.try_read() != std::optional{llab::BboSnapshot{}})
        throw std::runtime_error("initial snapshot");
    const llab::BboSnapshot published{100, 101, 102, 103};
    cell.publish(published);
    if (cell.try_read() != std::optional{published})
        throw std::runtime_error("published snapshot");

    std::atomic<bool> writer_done = false;
    std::thread writer([&] {
        for (std::uint64_t value = 1; value <= 100'000; ++value)
            cell.publish({value, value + 1, value + 2, value + 3});
        writer_done.store(true, std::memory_order_release);
    });
    while (!writer_done.load(std::memory_order_acquire))
        if (const auto snapshot = cell.try_read(); snapshot)
            if (snapshot->bid_quantity != snapshot->bid_price + 1 ||
                snapshot->ask_price != snapshot->bid_price + 2 ||
                snapshot->ask_quantity != snapshot->bid_price + 3)
                throw std::runtime_error("torn snapshot");
    writer.join();
}
