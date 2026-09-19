//! Operational telemetry, not a language benchmark. `cts` is Bybit matching-
//! engine time; receipt minus `cts` includes unknown inter-clock offset.
use futures_util::{SinkExt, StreamExt};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use tokio_tungstenite::{connect_async, tungstenite::Message};

fn integer(json: &str, key: &str) -> Option<i128> {
    let needle = format!("\"{key}\":"); let rest = &json[json.find(&needle)? + needle.len()..];
    let end = rest.find(|c: char| !c.is_ascii_digit()).unwrap_or(rest.len()); rest[..end].parse().ok()
}
fn rank(values: &mut [f64], p: f64) -> f64 { values.sort_by(f64::total_cmp); values[((values.len() as f64 * p).ceil() as usize - 1).min(values.len()-1)] }
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    rustls::crypto::ring::default_provider().install_default().ok();
    let (mut socket, _) = connect_async("wss://stream.bybit.com/v5/public/spot").await?;
    socket.send(Message::Text(r#"{"op":"subscribe","args":["orderbook.50.BTCUSDT"]}"#.into())).await?;
    let mut apparent_ms = Vec::with_capacity(50);
    while apparent_ms.len() < 50 {
        let next = tokio::time::timeout(Duration::from_secs(10), socket.next()).await?;
        let frame = next.ok_or("closed")??;
        if let Message::Text(text) = frame {
            if let Some(cts) = integer(&text, "cts") {
                let local = SystemTime::now().duration_since(UNIX_EPOCH)?.as_millis() as i128;
                apparent_ms.push((local - cts) as f64);
            }
        }
    }
    let mut p50 = apparent_ms.clone(); let mut p99 = apparent_ms.clone();
    println!("bybit_l2_apparent_cts_to_receive_ms samples=50 p50={:.3} p99={:.3} min={:.3} max={:.3} clock_offset_unknown=true", rank(&mut p50, 0.50), rank(&mut p99, 0.99), apparent_ms.iter().copied().fold(f64::INFINITY, f64::min), apparent_ms.iter().copied().fold(f64::NEG_INFINITY, f64::max));
    Ok(())
}
