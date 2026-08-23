#include "llab/metrics.hpp"

#include <stdexcept>

int main() {
    llab::metrics::ThreadRing ring(1);
    { llab::metrics::Scope<true> scope(&ring, 7); }
    if (ring.used() != 1 || ring.storage().front().tag != 7 ||
        ring.storage().front().end < ring.storage().front().begin)
        throw std::runtime_error("metrics record");
    ring.record(8, 1, 2, 0);
    if (ring.dropped() != 1)
        throw std::runtime_error("metrics drop");
}
