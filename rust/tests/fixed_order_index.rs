use low_latency_lab_benchmarks::fixed_order_index::FixedOrderIndex;
use std::collections::HashMap;

fn mix(mut value: u64) -> u64 {
    value = value.wrapping_add(0x9E37_79B9_7F4A_7C15);
    value = (value ^ (value >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    value = (value ^ (value >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    value ^ (value >> 31)
}

fn colliding_ids() -> [u64; 4] {
    let mut buckets = vec![Vec::new(); 8];
    for id in 1..10_000 {
        let bucket = &mut buckets[(mix(id) & 7) as usize];
        bucket.push(id);
        if bucket.len() == 4 {
            return [bucket[0], bucket[1], bucket[2], bucket[3]];
        }
    }
    panic!("failed to find collision cluster");
}

#[test]
fn collision_cluster_survives_deletion() {
    let mut index = FixedOrderIndex::new(4);
    let ids = colliding_ids();
    for (value, id) in ids.iter().enumerate() {
        assert!(index.insert(*id, 100 + value as u64));
    }

    assert_eq!(index.erase(ids[1]), Some(101));
    assert_eq!(index.find(ids[0]), Some(&100));
    assert_eq!(index.find(ids[2]), Some(&102));
    assert_eq!(index.find(ids[3]), Some(&103));
    assert_eq!(index.find(ids[1]), None);

    assert_eq!(index.erase(ids[0]), Some(100));
    assert_eq!(index.erase(ids[3]), Some(103));
    assert_eq!(index.find(ids[2]), Some(&102));
}

#[test]
fn long_history_preserves_capacity() {
    let mut index = FixedOrderIndex::new(32);
    for generation in 0..2_000 {
        let id = generation + 1;
        assert!(index.insert(id, generation));
        assert_eq!(index.erase(id), Some(generation));
    }
    assert_eq!(index.size(), 0);
    for id in 1..=32 {
        assert!(index.insert(id, id * 10));
    }
    for id in 1..=32 {
        assert_eq!(index.find(id), Some(&(id * 10)));
    }
}

#[test]
fn randomized_history_matches_reference_model() {
    let mut index = FixedOrderIndex::new(64);
    let mut reference = HashMap::new();
    let mut random = 0xD1B5_4A32_D192_ED03_u64;
    for _ in 0..50_000 {
        random =
            random.wrapping_mul(6_364_136_223_846_793_005).wrapping_add(1_442_695_040_888_963_407);
        let id = 1 + (random >> 16) % 128;
        let value = random ^ (random >> 29);
        match random & 3 {
            0 if reference.len() < 64 && !reference.contains_key(&id) => {
                assert!(index.insert(id, value));
                reference.insert(id, value);
            }
            1 => {
                assert_eq!(index.erase(id), reference.remove(&id));
            }
            _ => {
                assert_eq!(index.find(id), reference.get(&id));
            }
        }
        assert_eq!(index.size(), reference.len());
    }
}

#[test]
fn iteration_visits_only_live_entries() {
    let mut index = FixedOrderIndex::new(4);
    assert!(index.insert(1, 11_u32));
    assert!(index.insert(2, 22_u32));
    let mut sum = 0;
    index.for_each(|_, value| sum += value);
    assert_eq!(sum, 33);
    let probes = index.probe_summary();
    assert_eq!(probes.occupied, 2);
    assert!(probes.total_probes >= 2 && probes.maximum_probes >= 1);
}
