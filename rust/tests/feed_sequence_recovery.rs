use low_latency_lab_benchmarks::feed_sequence_recovery::{
    FeedSequenceRecovery, IncrementalResult, State,
};

#[test]
fn replays_a_contiguous_suffix_and_ignores_duplicates() {
    let mut recovery = FeedSequenceRecovery::<u64>::new(4);
    let mut ready = Vec::new();
    recovery.begin_snapshot(10);
    assert_eq!(recovery.on_incremental(12, 12, &mut ready), IncrementalResult::Buffered);
    assert_eq!(recovery.on_incremental(11, 11, &mut ready), IncrementalResult::Buffered);
    assert_eq!(recovery.on_incremental(11, 111, &mut ready), IncrementalResult::Buffered);
    assert!(recovery.finish_snapshot(&mut ready));
    assert_eq!(ready, vec![11, 12]);
    assert_eq!(recovery.expected_sequence(), Some(13));
    assert_eq!(recovery.on_incremental(12, 12, &mut ready), IncrementalResult::IgnoreStale);
    assert_eq!(recovery.on_incremental(14, 14, &mut ready), IncrementalResult::Gap);
    assert_eq!(recovery.state(), State::AwaitingSnapshot);
}

#[test]
fn refuses_partial_replay_and_buffer_overflow() {
    let mut recovery = FeedSequenceRecovery::<u64>::new(4);
    let mut ready = Vec::new();
    recovery.begin_snapshot(20);
    assert_eq!(recovery.on_incremental(22, 22, &mut ready), IncrementalResult::Buffered);
    assert!(!recovery.finish_snapshot(&mut ready));
    assert_eq!(recovery.state(), State::AwaitingSnapshot);

    recovery.begin_snapshot(30);
    for sequence in 31..=34 {
        assert_eq!(
            recovery.on_incremental(sequence, sequence, &mut ready),
            IncrementalResult::Buffered
        );
    }
    assert_eq!(recovery.on_incremental(35, 35, &mut ready), IncrementalResult::BufferFull);
    assert_eq!(recovery.state(), State::AwaitingSnapshot);
}
