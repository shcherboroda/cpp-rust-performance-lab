use futures_util::{SinkExt, StreamExt};
use std::time::Instant;
use tokio_tungstenite::{connect_async, tungstenite::Message};

#[tokio::main]
async fn main() {
    rustls::crypto::ring::default_provider()
        .install_default()
        .expect("install rustls ring provider");

    let started = Instant::now();
    let (mut socket, _) = connect_async("wss://api-pub.bitfinex.com/ws/2").await.expect("connect");
    println!("connect_ms={:.3}", started.elapsed().as_secs_f64() * 1e3);
    socket.send(Message::Text(r#"{"event":"subscribe","channel":"book","symbol":"tBTCUSD","prec":"R0","freq":"F0","len":"25"}"#.into())).await.expect("subscribe");
    for index in 0..4 {
        match socket.next().await.expect("stream").expect("frame") {
            Message::Text(frame) => println!(
                "frame={index} receive_ms={:.3} bytes={} payload={frame}",
                started.elapsed().as_secs_f64() * 1e3,
                frame.len()
            ),
            frame => println!("frame={index} non_text={frame:?}"),
        }
    }
}
