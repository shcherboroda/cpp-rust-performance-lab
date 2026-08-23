#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum State {
    Live,
    AwaitingSnapshot,
    Snapshotting,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum IncrementalResult {
    Apply,
    IgnoreStale,
    Gap,
    Buffered,
    BufferFull,
}

#[derive(Clone, Copy)]
struct Buffered<Event: Copy + Default> {
    sequence: u64,
    event: Event,
}

impl<Event: Copy + Default> Default for Buffered<Event> {
    fn default() -> Self {
        Self { sequence: 0, event: Event::default() }
    }
}

/// Bounded single-writer sequence and snapshot-replay state machine.
///
/// Venue adapters normalize their protocol into this component and apply only
/// events returned in `ready`. See `specs/feed_sequence_recovery_v1.md`.
pub struct FeedSequenceRecovery<Event: Copy + Default> {
    replay: Vec<Buffered<Event>>,
    replay_size: usize,
    state: State,
    expected: u64,
    snapshot_last: u64,
}

impl<Event: Copy + Default> FeedSequenceRecovery<Event> {
    pub fn new(maximum_replay_events: usize) -> Self {
        Self {
            replay: vec![Buffered::default(); maximum_replay_events],
            replay_size: 0,
            state: State::AwaitingSnapshot,
            expected: 0,
            snapshot_last: 0,
        }
    }

    pub fn state(&self) -> State {
        self.state
    }

    pub fn expected_sequence(&self) -> Option<u64> {
        (self.state == State::Live).then_some(self.expected)
    }

    pub fn on_incremental(
        &mut self,
        sequence: u64,
        event: Event,
        ready: &mut Vec<Event>,
    ) -> IncrementalResult {
        match self.state {
            State::AwaitingSnapshot => IncrementalResult::Gap,
            State::Snapshotting => {
                if sequence <= self.snapshot_last {
                    IncrementalResult::IgnoreStale
                } else if self.replay_size == self.replay.len() {
                    self.state = State::AwaitingSnapshot;
                    IncrementalResult::BufferFull
                } else {
                    self.replay[self.replay_size] = Buffered { sequence, event };
                    self.replay_size += 1;
                    IncrementalResult::Buffered
                }
            }
            State::Live if sequence < self.expected => IncrementalResult::IgnoreStale,
            State::Live if sequence > self.expected => {
                self.state = State::AwaitingSnapshot;
                IncrementalResult::Gap
            }
            State::Live => {
                ready.push(event);
                self.expected += 1;
                IncrementalResult::Apply
            }
        }
    }

    pub fn begin_snapshot(&mut self, last_sequence: u64) {
        self.state = State::Snapshotting;
        self.snapshot_last = last_sequence;
        self.replay_size = 0;
    }

    pub fn finish_snapshot(&mut self, ready: &mut Vec<Event>) -> bool {
        if self.state != State::Snapshotting {
            return false;
        }
        self.replay[..self.replay_size].sort_unstable_by_key(|entry| entry.sequence);
        let expected = self.snapshot_last + 1;
        let first =
            self.replay[..self.replay_size].partition_point(|entry| entry.sequence < expected);
        let mut next = expected;
        for entry in &self.replay[first..self.replay_size] {
            if entry.sequence < next {
                continue;
            }
            if entry.sequence > next {
                self.state = State::AwaitingSnapshot;
                self.replay_size = 0;
                return false;
            }
            next += 1;
        }
        let mut emit_sequence = expected;
        for entry in &self.replay[first..self.replay_size] {
            if entry.sequence == emit_sequence {
                ready.push(entry.event);
                emit_sequence += 1;
            }
        }
        self.replay_size = 0;
        self.expected = next;
        self.state = State::Live;
        true
    }
}
