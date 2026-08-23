#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct CycleSample {
    pub begin: u64,
    pub end: u64,
    pub tag: u32,
    pub cpu: u32,
}

pub struct ThreadRing {
    samples: Vec<CycleSample>,
    used: usize,
    dropped: usize,
}

impl ThreadRing {
    pub fn with_capacity(capacity: usize) -> Self {
        assert!(capacity > 0, "metric ring capacity");
        Self { samples: vec![CycleSample::default(); capacity], used: 0, dropped: 0 }
    }

    pub fn record(&mut self, tag: u32, begin: u64, end: u64, cpu: u32) {
        if self.used == self.samples.len() {
            self.dropped += 1;
            return;
        }
        self.samples[self.used] = CycleSample { begin, end, tag, cpu };
        self.used += 1;
    }

    pub fn samples(&self) -> &[CycleSample] {
        &self.samples[..self.used]
    }
    pub fn dropped(&self) -> usize {
        self.dropped
    }
    pub fn reset(&mut self) {
        self.used = 0;
        self.dropped = 0;
    }
}

#[cfg(target_arch = "x86_64")]
pub fn cycle_begin() -> u64 {
    unsafe {
        core::arch::x86_64::_mm_lfence();
        core::arch::x86_64::_rdtsc()
    }
}

#[cfg(target_arch = "x86_64")]
pub fn cycle_end() -> (u64, u32) {
    unsafe {
        let mut cpu = 0;
        let value = core::arch::x86_64::__rdtscp(&mut cpu);
        core::arch::x86_64::_mm_lfence();
        (value, cpu)
    }
}

#[cfg(not(target_arch = "x86_64"))]
pub fn cycle_begin() -> u64 {
    0
}
#[cfg(not(target_arch = "x86_64"))]
pub fn cycle_end() -> (u64, u32) {
    (0, 0)
}

pub fn record_scope(ring: &mut ThreadRing, tag: u32, begin: u64) {
    let (end, cpu) = cycle_end();
    ring.record(tag, begin, end, cpu);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn records_without_growing_or_blocking() {
        let mut ring = ThreadRing::with_capacity(1);
        let begin = cycle_begin();
        record_scope(&mut ring, 7, begin);
        assert_eq!(ring.samples().len(), 1);
        assert_eq!(ring.samples()[0].tag, 7);
        assert!(ring.samples()[0].end >= ring.samples()[0].begin);
        ring.record(8, 1, 2, 0);
        assert_eq!(ring.dropped(), 1);
    }
}
