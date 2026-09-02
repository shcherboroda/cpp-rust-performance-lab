pub const HEADER_SIZE: usize = 38;
pub const MAX_PAYLOAD: usize = 1 << 20;
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RawFrameRecord {
    pub capture_index: u64,
    pub monotonic_ns: u64,
    pub utc_ns: i64,
    pub connection_id: u64,
    pub direction: u8,
    pub kind: u8,
    pub payload: Vec<u8>,
}
pub fn crc32c(bytes: &[u8]) -> u32 {
    let mut value = !0u32;
    for byte in bytes {
        value ^= *byte as u32;
        for _ in 0..8 {
            value = (value >> 1) ^ (0x82f63b78 & (0u32.wrapping_sub(value & 1)));
        }
    }
    !value
}
fn put<T: Into<u64>>(out: &mut Vec<u8>, value: T, n: usize) {
    let value = value.into();
    for i in 0..n {
        out.push((value >> (8 * i)) as u8);
    }
}
pub fn encode(record: &RawFrameRecord) -> Vec<u8> {
    let mut out = Vec::with_capacity(HEADER_SIZE + record.payload.len() + 4);
    put(&mut out, record.capture_index, 8);
    put(&mut out, record.monotonic_ns, 8);
    put(&mut out, record.utc_ns as u64, 8);
    put(&mut out, record.connection_id, 8);
    out.push(record.direction);
    out.push(record.kind);
    put(&mut out, record.payload.len() as u64, 4);
    out.extend_from_slice(&record.payload);
    let checksum = crc32c(&out);
    put(&mut out, checksum as u64, 4);
    out
}
fn get(bytes: &[u8], offset: usize, n: usize) -> u64 {
    (0..n).fold(0, |v, i| v | ((bytes[offset + i] as u64) << (8 * i)))
}
pub fn decode(bytes: &[u8]) -> Option<RawFrameRecord> {
    if bytes.len() < HEADER_SIZE + 4 {
        return None;
    }
    let n = get(bytes, 34, 4) as usize;
    if n > MAX_PAYLOAD
        || bytes.len() != HEADER_SIZE + n + 4
        || crc32c(&bytes[..bytes.len() - 4]) != get(bytes, bytes.len() - 4, 4) as u32
    {
        return None;
    }
    Some(RawFrameRecord {
        capture_index: get(bytes, 0, 8),
        monotonic_ns: get(bytes, 8, 8),
        utc_ns: get(bytes, 16, 8) as i64,
        connection_id: get(bytes, 24, 8),
        direction: bytes[32],
        kind: bytes[33],
        payload: bytes[HEADER_SIZE..HEADER_SIZE + n].to_vec(),
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn round_trip_and_corruption() {
        let r = RawFrameRecord {
            capture_index: 7,
            monotonic_ns: 9,
            utc_ns: -1,
            connection_id: 11,
            direction: 0,
            kind: 0,
            payload: b"[1,2]".to_vec(),
        };
        let mut b = encode(&r);
        assert_eq!(decode(&b), Some(r));
        b[0] ^= 1;
        assert_eq!(decode(&b), None);
    }
}
