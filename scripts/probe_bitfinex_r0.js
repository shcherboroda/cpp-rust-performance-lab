#!/usr/bin/env node

const { performance } = require("perf_hooks");
const WebSocket = require("ws");

const connections = Number.parseInt(process.env.LLAB_PROBE_CONNECTIONS ?? "8", 10);
const durationMs = Number.parseInt(process.env.LLAB_PROBE_DURATION_MS ?? "8000", 10);
const endpoint = process.env.BITFINEX_WS_ENDPOINT ?? "wss://api-pub.bitfinex.com/ws/2";
const symbol = process.env.BITFINEX_SYMBOL ?? "tBTCUSD";
const length = process.env.BITFINEX_BOOK_LENGTH ?? "25";

function quantile(values, percentile) {
    if (values.length === 0) return null;
    const sorted = [...values].sort((left, right) => left - right);
    return sorted[Math.min(sorted.length - 1, Math.ceil(sorted.length * percentile) - 1)];
}

function summary(values) {
    return {
        n: values.length,
        p50: quantile(values, 0.50),
        p95: quantile(values, 0.95),
        p99: quantile(values, 0.99),
        p999: quantile(values, 0.999),
        max: values.length === 0 ? null : Math.max(...values),
    };
}

const results = [];
for (let index = 0; index < connections; index += 1) {
    const started = performance.now();
    const socket = new WebSocket(endpoint);
    let connected;
    let subscribed;
    let snapshot;
    let firstUpdate;
    let previousUpdate;
    const interarrivalMs = [];
    let updates = 0;

    socket.on("open", () => {
        connected = performance.now();
        socket.send(JSON.stringify({
            event: "subscribe", channel: "book", symbol, prec: "R0", freq: "F0", len: length,
        }));
        setTimeout(() => {
            results.push({ connected, subscribed, snapshot, firstUpdate, started, updates, interarrivalMs });
            socket.terminate();
            if (results.length !== connections) return;
            const elapsed = name => results.map(item => item[name] - item.started).filter(Number.isFinite);
            console.log(JSON.stringify({
                endpoint, symbol, length, connections, duration_ms: durationMs,
                connect_ms: summary(elapsed("connected")),
                subscribe_ack_ms: summary(elapsed("subscribed")),
                snapshot_ms: summary(elapsed("snapshot")),
                first_market_update_ms: summary(elapsed("firstUpdate")),
                updates_per_connection: results.map(item => item.updates),
                market_update_interarrival_ms: summary(results.flatMap(item => item.interarrivalMs)),
            }, null, 2));
        }, durationMs);
    });

    socket.on("message", raw => {
        const now = performance.now();
        const message = JSON.parse(raw);
        if (message.event === "subscribed") subscribed = now;
        if (!Array.isArray(message) || !Array.isArray(message[1])) return;
        if (Array.isArray(message[1][0])) {
            snapshot ??= now;
            return;
        }
        firstUpdate ??= now;
        if (previousUpdate !== undefined) interarrivalMs.push(now - previousUpdate);
        previousUpdate = now;
        updates += 1;
    });

    socket.on("error", error => console.error(`connection ${index}: ${error.message}`));
}
